#include "z3_verifier.h"
#include "error_reporter.h"
#include <iostream>
#include <stdexcept>
#include <future>
#include <atomic>
#include <mutex>

static z3::expr ensure_bool(const z3::expr& e, z3::context& ctx) {
    if (e.is_bool()) return e;
    if (e.is_int()) return e != 0;
    return e;
}

Z3Verifier::Z3Verifier() : solver(ctx), memory_ownership(ctx), var_alive(ctx), path_guard(ctx.bool_val(true)) {
    solver.set("timeout", 10000u);
    memory_ownership = ctx.constant("memory_ownership_0", ctx.array_sort(ctx.int_sort(), ctx.int_sort()));
    var_alive = ctx.constant("var_alive_0", ctx.array_sort(ctx.int_sort(), ctx.bool_sort()));
}

bool Z3Verifier::hasCounterexample() {
    solver.add(path_guard);
    const auto result = solver.check();
    if (result == z3::unknown) {
        throw std::runtime_error(ErrorReporter::formatError(
            "Z3 Verification Inconclusive: " + solver.reason_unknown(),
            current_node ? current_node->file : "",
            current_node ? current_node->line : 1,
            current_node ? current_node->col : 1));
    }
    return result == z3::sat;
}

z3::expr Z3Verifier::freshInt(const std::string& prefix) {
    return z3::expr(ctx, Z3_mk_fresh_const(ctx, prefix.c_str(), ctx.int_sort()));
}

void Z3Verifier::pushScope() {
    saved_var_ids.push_back(var_to_id);
    saved_bounds.push_back(bounds_table);
    scope_var_ids.emplace_back();
    scope_stack.emplace_back();
    owned_pointers_stack.emplace_back();
    solver.push();
}

void Z3Verifier::checkLeaks(size_t first_scope) {
    for (size_t scope = first_scope; scope < scope_stack.size(); ++scope) {
        for (const auto& name : owned_pointers_stack[scope]) {
            const auto value = scope_stack[scope].find(name);
            const auto id = scope_var_ids[scope].find(name);
            if (value == scope_stack[scope].end() || id == scope_var_ids[scope].end()) continue;
            solver.push();
            solver.add(z3::select(var_alive, ctx.int_val(id->second)) &&
                       z3::select(memory_ownership, value->second) == 1);
            if (hasCounterexample()) {
                throw std::runtime_error(ErrorReporter::formatError(
                    "Z3 Verification Failed: owned memory leaks at scope exit: " + name,
                    current_node ? current_node->file : "",
                    current_node ? current_node->line : 1,
                    current_node ? current_node->col : 1));
            }
            solver.pop();
        }
    }
}

void Z3Verifier::popScope() {
    checkLeaks(scope_stack.size() - 1);
    owned_pointers_stack.pop_back();
    scope_stack.pop_back();
    scope_var_ids.pop_back();
    var_to_id = std::move(saved_var_ids.back());
    saved_var_ids.pop_back();
    bounds_table = std::move(saved_bounds.back());
    saved_bounds.pop_back();
    solver.pop();
}

void Z3Verifier::declareVar(const std::string& name, const z3::expr& val) {
    if (!scope_stack.empty()) scope_stack.back().insert_or_assign(name, val);
}

void Z3Verifier::assignVar(const std::string& name, const z3::expr& val) {
    for (auto scope = scope_stack.rbegin(); scope != scope_stack.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            found->second = val;
            return;
        }
    }
    declareVar(name, val);
}

Z3Verifier::FlowState Z3Verifier::saveFlow() const {
    return {scope_stack, owned_pointers_stack, memory_ownership, var_alive, path_guard};
}

void Z3Verifier::restoreFlow(const FlowState& state) {
    scope_stack = state.variables;
    owned_pointers_stack = state.owners;
    memory_ownership = state.heap;
    var_alive = state.liveness;
    path_guard = state.path;
}

void Z3Verifier::mergeFlow(const z3::expr& condition, const FlowState& then_state,
                           const FlowState& else_state) {
    scope_stack = then_state.variables;
    owned_pointers_stack = then_state.owners;
    for (size_t i = 0; i < scope_stack.size(); ++i) {
        // Fields may first be assigned in either branch. Include both sets,
        // leaving an uninitialized branch unconstrained.
        for (const auto& binding : else_state.variables[i]) {
            if (!scope_stack[i].count(binding.first)) {
                scope_stack[i].emplace(binding.first, z3::expr(ctx,
                    Z3_mk_fresh_const(ctx, "uninitialized_field", binding.second.get_sort())));
            }
        }
        for (auto& binding : scope_stack[i]) {
            auto other = else_state.variables[i].find(binding.first);
            z3::expr else_value = other == else_state.variables[i].end()
                ? z3::expr(ctx, Z3_mk_fresh_const(ctx, "uninitialized_field", binding.second.get_sort()))
                : other->second;
            binding.second = z3::ite(condition, binding.second, else_value).simplify();
        }
        owned_pointers_stack[i].insert(else_state.owners[i].begin(), else_state.owners[i].end());
    }
    memory_ownership = z3::ite(condition, then_state.heap, else_state.heap);
    var_alive = z3::ite(condition, then_state.liveness, else_state.liveness);
    path_guard = (then_state.path || else_state.path).simplify();
}

z3::expr Z3Verifier::getVar(const std::string& name) {
    for (int i = (int)scope_stack.size() - 1; i >= 0; --i) {
        auto it = scope_stack[i].find(name);
        if (it != scope_stack[i].end()) {
            return it->second;
        }
    }
    // If not found, create a fresh symbolic variable
    return ctx.int_const(name.c_str());
}

void Z3Verifier::verifyBounds(ASTNode* arrayExpr, ASTNode* indexExpr) {
    if (auto varAccess = dynamic_cast<VarAccessNode*>(arrayExpr)) {
        auto it = bounds_table.find(varAccess->name);
        if (it != bounds_table.end()) {
            z3::expr size_expr = it->second;
            z3::expr idx_expr = evalExpression(indexExpr);
            
            // We want to prove: idx >= 0 && idx < size
            // We ask Z3 if the NEGATION is satisfiable
            z3::expr overflow_condition = !(idx_expr >= 0 && idx_expr < size_expr);
            
            solver.push();
            solver.add(overflow_condition);
            
            if (hasCounterexample()) {
                z3::model m = solver.get_model();
//                 std::cerr << "\n[ALU CXX Z3 FATAL] Mathematical Memory Bounds Violation Detected!" << std::endl;
//                 std::cerr << "  Array: '" << varAccess->name << "'" << std::endl;
//                 std::cerr << "  Z3 Counterexample: " << m << std::endl;
                int l = current_node ? current_node->line : 1;
                int c = current_node ? current_node->col : 1;
                std::string f = current_node ? current_node->file : "";
                throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
            }
            solver.pop();
        }
    }
}

void Z3Verifier::verifyPointerValid(const z3::expr& ptrExpr, const std::string& contextMsg, bool require_ownership) {
    z3::expr state = z3::select(memory_ownership, ptrExpr);
    z3::expr violation_condition = require_ownership ? (state != 1) : (state != 1 && state != 3);
    
    solver.push();
    solver.add(violation_condition);
    
    if (hasCounterexample()) {
        z3::model m = solver.get_model();
//         std::cerr << "\n[ALU CXX Z3 FATAL] Use-After-Free / Invalid Pointer Violation Detected!" << std::endl;
//         std::cerr << "  Context: " << contextMsg << std::endl;
//         std::cerr << "  Z3 Counterexample: " << m << std::endl;
        int l = current_node ? current_node->line : 1;
        int c = current_node ? current_node->col : 1;
        std::string f = current_node ? current_node->file : "";
        throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
    }
    solver.pop();
}

void Z3Verifier::verifyDivisionByZero(const z3::expr& denominator) {
    // To verify the denominator is never zero, we ask Z3 if `denominator == 0` is satisfiable
    solver.push();
    solver.add(denominator == 0);
    
    if (hasCounterexample()) {
        z3::model m = solver.get_model();
//         std::cerr << "\n[ALU CXX Z3 FATAL] Mathematical Division by Zero Detected!" << std::endl;
//         std::cerr << "  Z3 Counterexample: " << m << std::endl;
        int l = current_node ? current_node->line : 1;
        int c = current_node ? current_node->col : 1;
        std::string f = current_node ? current_node->file : "";
        throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
    }
    solver.pop();
}

// --- Contract Helpers ---

bool Z3Verifier::isStringLiteralAnnotation(ASTNode* expr) {
    if (auto lit = dynamic_cast<LiteralNode*>(expr)) {
        return lit->type == DataType::STRING;
    }
    return false;
}

// Evaluate an annotation expression, substituting formal parameter names
// with the corresponding Z3 expressions from actual arguments.
// Also handles the special variable "return" mapped to __return.
z3::expr Z3Verifier::evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args, const std::vector<z3::expr>& actual_values) {
    pushScope();
    for (size_t i = 0; i < formal_params.size() && i < actual_args.size(); ++i) {
        // Values were evaluated in the caller scope, before any formal names
        // can shadow variables in later arguments.
        z3::expr arg_val = actual_values.at(i);
        declareVar(formal_params[i].name, arg_val);
        if (!formal_params[i].refinement_var.empty()) {
            declareVar(formal_params[i].refinement_var, arg_val);
        }
        
        // Map struct fields
        if (auto varNode = dynamic_cast<VarAccessNode*>(actual_args[i])) {
            std::string prefix = varNode->name + "_";
            std::string target_prefix = formal_params[i].name + "_";
            for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
                for (const auto& kv : *it) {
                    if (kv.first.find(prefix) == 0) {
                        std::string field_name = kv.first.substr(prefix.length());
                        declareVar(target_prefix + field_name, kv.second);
                    }
                }
            }
        }
    }
    z3::expr result = ensure_bool(evalExpression(expr), ctx);
    popScope();
    return result;
}

// --- Contract Registration Pass ---

void Z3Verifier::registerContracts(ProgramNode* node) {
    registerContractsInDeclarations(node->declarations);
}

void Z3Verifier::registerContractsInDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (const auto& decl : declarations) {
        if (auto routine = dynamic_cast<RoutineNode*>(decl.get())) {
            if (!routine->requires_annotations.empty() || !routine->ensures_annotations.empty()) {
                RoutineContract contract;
                contract.name = routine->name;
                contract.params = routine->params;
                contract.returnType = routine->returnType;
                for (const auto& req : routine->requires_annotations) {
                    contract.requires_exprs.push_back(req.get());
                }
                for (const auto& ens : routine->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }
                for (const auto& p : routine->params) {
                    if (p.refinement_expr) {
                        contract.requires_exprs.push_back(p.refinement_expr.get());
                    }
                }
                routine_contracts[routine->name] = contract;
            }
        } else if (auto ext = dynamic_cast<ExternRoutineNode*>(decl.get())) {
            if (!ext->requires_annotations.empty() || !ext->ensures_annotations.empty()) {
                RoutineContract contract;
                contract.name = ext->name;
                contract.params = ext->params;
                contract.returnType = ext->returnType;
                for (const auto& req : ext->requires_annotations) {
                    contract.requires_exprs.push_back(req.get());
                }
                for (const auto& ens : ext->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }
                for (const auto& p : ext->params) {
                    if (p.refinement_expr) {
                        contract.requires_exprs.push_back(p.refinement_expr.get());
                    }
                }
                routine_contracts[ext->name] = contract;
            }
        } else if (auto nsNode = dynamic_cast<NamespaceNode*>(decl.get())) {
            registerContractsInDeclarations(nsNode->declarations);
        }
    }
}

// --- Precondition Verification at Call Sites ---

void Z3Verifier::verifyRequiresAtCallSite(const std::string& calleeName,
                                           const std::vector<std::unique_ptr<ASTNode>>& actual_args) {
    std::vector<z3::expr> actual_values;
    for (const auto& arg : actual_args) actual_values.push_back(evalExpression(arg.get()));
    auto it = routine_contracts.find(calleeName);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.requires_exprs.empty()) return;

    std::vector<ASTNode*> args_ptrs;
    for (const auto& arg : actual_args) args_ptrs.push_back(arg.get());

    // Check each @requires clause
    for (ASTNode* req_expr : contract.requires_exprs) {
        if (isStringLiteralAnnotation(req_expr)) continue;

        z3::expr precondition = evalAnnotationExpr(req_expr, contract.params, args_ptrs, actual_values);
        
        // To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!precondition);
        
        if (hasCounterexample()) {
            z3::model m = solver.get_model();
            std::cerr << "\n[ALU CXX Z3 FATAL] @requires Contract Violation Detected!" << std::endl;
            std::cerr << "  Function: '" << calleeName << "'" << std::endl;
            std::cerr << "  Precondition may not hold at this call site." << std::endl;
            std::cerr << "  Z3 Counterexample: " << m << std::endl;
            int l = current_node ? current_node->line : 1;
            int c = current_node ? current_node->col : 1;
            std::string f = current_node ? current_node->file : "";
            throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
        }
        solver.pop();
    }
}

// --- Postcondition Verification at Return Statements ---

void Z3Verifier::verifyEnsuresAtReturn(RoutineNode* routine, const z3::expr& ret_val) {
    auto it = routine_contracts.find(routine->name);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.ensures_exprs.empty()) return;

    std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
    std::vector<ASTNode*> param_exprs;
    std::vector<z3::expr> param_values;
    for (const auto& p : contract.params) {
        param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
        param_exprs.push_back(param_exprs_mem.back().get());
        param_values.push_back(getVar(p.name));
    }

    // The return expression was evaluated once, before postcondition binding.
    pushScope();
    declareVar("__return", ret_val);

    // Check each @ensures clause
    for (ASTNode* ens_expr : contract.ensures_exprs) {
        // Skip string-literal annotations (documentation only)
        if (isStringLiteralAnnotation(ens_expr)) continue;

        z3::expr postcondition = evalAnnotationExpr(ens_expr, contract.params, param_exprs, param_values);
        
        // To verify the postcondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!postcondition);
        
        if (hasCounterexample()) {
            z3::model m = solver.get_model();
//             std::cerr << "\n[ALU CXX Z3 FATAL] @ensures Contract Violation Detected!" << std::endl;
//             std::cerr << "  Function: '" << routine->name << "'" << std::endl;
//             std::cerr << "  Postcondition may not hold for this return value." << std::endl;
//             std::cerr << "  Z3 Counterexample: " << m << std::endl;
            int l = current_node ? current_node->line : 1;
            int c = current_node ? current_node->col : 1;
            std::string f = current_node ? current_node->file : "";
            throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
        }
        solver.pop();
    }
    popScope();
}

// --- Expression Evaluation ---

z3::expr Z3Verifier::evalExpression(ASTNode* expr) {
    if (auto literal = dynamic_cast<LiteralNode*>(expr)) {
        if (literal->type == DataType::INT) {
            return ctx.int_val(std::stoll(literal->value));
        } else if (literal->type == DataType::BOOL) {
            return ctx.bool_val(literal->value == "true");
        }
    } else if (auto varAccess = dynamic_cast<VarAccessNode*>(expr)) {
        if (varAccess->name == "return" || varAccess->name == "__return") {
            return getVar("__return");
        }
        
        if (var_to_id.count(varAccess->name)) {
            int v_id = var_to_id[varAccess->name];
            z3::expr is_alive = z3::select(var_alive, ctx.int_val(v_id));
            solver.push();
            solver.add(!is_alive);
            if (hasCounterexample()) {
                z3::model m = solver.get_model();
//                 std::cerr << "\n[ALU CXX Z3 FATAL] Use-After-Move Violation Detected!" << std::endl;
//                 std::cerr << "  Variable: '" << varAccess->name << "' was moved or freed." << std::endl;
//                 std::cerr << "  Z3 Counterexample: " << m << std::endl;
                int l = current_node ? current_node->line : 1;
                int c = current_node ? current_node->col : 1;
                std::string f = current_node ? current_node->file : "";
                throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
            }
            solver.pop();
        }
        return getVar(varAccess->name);
    } else if (auto memberAccess = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string full_name = memberAccess->objectName + "_" + memberAccess->fieldName;
        return getVar(full_name);
    } else if (auto binop = dynamic_cast<BinOpNode*>(expr)) {
        z3::expr left = evalExpression(binop->left.get());
        z3::expr right = evalExpression(binop->right.get());
        if (binop->op == "+") return left + right;
        if (binop->op == "-") return left - right;
        if (binop->op == "*") return left * right;
        if (binop->op == "/") {
            verifyDivisionByZero(right);
            return left / right;
        }
        if (binop->op == "==") return left == right;
        if (binop->op == "!=") return left != right;
        if (binop->op == "<") return left < right;
        if (binop->op == "<=") return left <= right;
        if (binop->op == ">") return left > right;
        if (binop->op == ">=") return left >= right;
        if (binop->op == "&&") return left && right;
        if (binop->op == "||") return left || right;
    }
    else if (auto arrIndex = dynamic_cast<ArrayIndexNode*>(expr)) {
        verifyBounds(arrIndex->arrayExpr.get(), arrIndex->indexExpr.get());
        return freshInt("array_value");
    } else if (auto deref = dynamic_cast<DereferenceNode*>(expr)) {
        z3::expr ptr_id = evalExpression(deref->expr.get());
        verifyPointerValid(ptr_id, "Dereference (Read)");
        return freshInt("deref_value");
    } else if (auto newAlloc = dynamic_cast<NewAllocationNode*>(expr)) {
        z3::expr ptr_id = ctx.int_val(next_alloc_id++);
        memory_ownership = z3::store(memory_ownership, ptr_id, ctx.int_val(1)); // 1 = Owned
        return ptr_id;
    }
    else if (auto funcCall = dynamic_cast<FuncCallNode*>(expr)) {
        // Verify @requires at this call site
        verifyRequiresAtCallSite(funcCall->name, funcCall->args);
        // Return a symbolic value representing the function's return
        return freshInt("call_result");
    }
    // Fallback to a dummy variable
    return freshInt("unmodeled_value");
}

// Explore every feasible iteration up to a bounded proof budget. Exhaustion
// is inconclusive, never a proof obtained by ignoring later iterations.
void Z3Verifier::checkLoop(ASTNode* condition,
                           const std::vector<std::unique_ptr<ASTNode>>& body,
                           ASTNode* update) {
    FlowState exits = saveFlow();
    exits.path = ctx.bool_val(false);
    for (size_t iteration = 0;; ++iteration) {
        if (path_guard.is_false()) {
            restoreFlow(exits);
            return;
        }
        const z3::expr cond = condition
            ? ensure_bool(evalExpression(condition), ctx) : ctx.bool_val(true);
        const FlowState before = saveFlow();
        FlowState exit_state = before;
        exit_state.path = (before.path && !cond).simplify();
        mergeFlow(exit_state.path, exit_state, exits);
        exits = saveFlow();
        restoreFlow(before);
        path_guard = (before.path && cond).simplify();

        solver.push();
        const bool reachable = hasCounterexample();
        solver.pop();
        if (!reachable) {
            restoreFlow(exits);
            return;
        }
        if (iteration >= 64 || loop_steps >= 1024) {
            throw std::runtime_error(ErrorReporter::formatError(
                "Z3 Verification Inconclusive: loop proof budget exhausted (64 iterations per loop, 1024 per routine)",
                current_node ? current_node->file : "",
                current_node ? current_node->line : 1,
                current_node ? current_node->col : 1));
        }
        ++loop_steps;
        pushScope();
        for (const auto& statement : body) checkStatement(statement.get());
        popScope();
        if (update) checkStatement(update);
    }
}

// --- Statement Checking ---

void Z3Verifier::checkStatement(ASTNode* stmt) {
    if (path_guard.is_false()) return;
    ASTNode* old_node = current_node;
    current_node = stmt;
    if (auto vardecl = dynamic_cast<VarDeclNode*>(stmt)) {
        // Bind the value expression itself: successive assignments must not add
        // contradictory equalities to one reused symbol and prove everything.
        z3::expr value = vardecl->initializer
            ? evalExpression(vardecl->initializer.get()) : freshInt(vardecl->name);
        const bool is_pointer = !vardecl->varType.empty() && vardecl->varType.back() == '*';
        if (is_pointer && vardecl->initializer) {
            if (auto source = dynamic_cast<VarAccessNode*>(vardecl->initializer.get())) {
                if (var_to_id.count(source->name)) {
                    var_alive = z3::store(var_alive, ctx.int_val(var_to_id.at(source->name)), ctx.bool_val(false));
                }
            }
        }

        int id = next_var_id++;
        var_to_id[vardecl->name] = id;
        scope_var_ids.back()[vardecl->name] = id;
        var_alive = z3::store(var_alive, ctx.int_val(id), ctx.bool_val(true));
        declareVar(vardecl->name, value);

        if (vardecl->refinement_expr && vardecl->initializer) {
            pushScope();
            declareVar(vardecl->refinement_var, value);
            z3::expr constraint = ensure_bool(evalExpression(vardecl->refinement_expr.get()), ctx);
            solver.push();
            solver.add(!constraint);
            if (hasCounterexample()) {
                throw std::runtime_error(ErrorReporter::formatError(
                    "Z3 Verification Failed: Refinement constraint not satisfied on assignment",
                    stmt->file, stmt->line, stmt->col));
            }
            solver.pop();
            popScope();
        }
        if (is_pointer) owned_pointers_stack.back().insert(vardecl->name);
    } else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        z3::expr value = evalExpression(memberAssign->expr.get());
        const std::string name = memberAssign->objectName + "_" + memberAssign->fieldName;
        // A field belongs to the object's declaring scope, not the branch in
        // which that field happened to be written.
        for (auto scope = scope_stack.rbegin(); scope != scope_stack.rend(); ++scope) {
            if (scope->count(memberAssign->objectName)) {
                scope->insert_or_assign(name, value);
                break;
            }
        }
    } else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        z3::expr value = evalExpression(varassign->expr.get());
        if (auto source = dynamic_cast<VarAccessNode*>(varassign->expr.get())) {
            bool owned_source = false;
            for (size_t i = scope_stack.size(); i-- > 0;) {
                if (scope_stack[i].count(source->name)) {
                    owned_source = owned_pointers_stack[i].count(source->name) != 0;
                    break;
                }
            }
            if (owned_source && var_to_id.count(source->name)) {
                var_alive = z3::store(var_alive, ctx.int_val(var_to_id.at(source->name)), ctx.bool_val(false));
                for (size_t i = scope_stack.size(); i-- > 0;) {
                    if (scope_stack[i].count(varassign->name)) {
                        owned_pointers_stack[i].insert(varassign->name);
                        break;
                    }
                }
            }
        }
        assignVar(varassign->name, value);
        if (var_to_id.count(varassign->name)) {
            var_alive = z3::store(var_alive, ctx.int_val(var_to_id.at(varassign->name)), ctx.bool_val(true));
        }
    } else if (auto arrDecl = dynamic_cast<ArrayDeclNode*>(stmt)) {
        z3::expr size_expr = evalExpression(arrDecl->sizeExpr.get());
        bounds_table.insert_or_assign(arrDecl->name, size_expr);
    } else if (auto arrAssign = dynamic_cast<ArrayAssignNode*>(stmt)) {
        verifyBounds(arrAssign->arrayExpr.get(), arrAssign->indexExpr.get());
        evalExpression(arrAssign->valExpr.get());
    } else if (auto arrIndex = dynamic_cast<ArrayIndexNode*>(stmt)) {
        verifyBounds(arrIndex->arrayExpr.get(), arrIndex->indexExpr.get());
    } else if (auto ifNode = dynamic_cast<IfNode*>(stmt)) {
        const z3::expr condition = ensure_bool(evalExpression(ifNode->condition.get()), ctx);
        const FlowState before = saveFlow();

        pushScope();
        path_guard = (before.path && condition).simplify();
        for (const auto& statement : ifNode->then_body) checkStatement(statement.get());
        popScope();
        const FlowState then_state = saveFlow();

        restoreFlow(before);
        pushScope();
        path_guard = (before.path && !condition).simplify();
        for (const auto& statement : ifNode->else_body) checkStatement(statement.get());
        popScope();
        const FlowState else_state = saveFlow();

        mergeFlow(condition, then_state, else_state);
    } else if (auto whileNode = dynamic_cast<WhileNode*>(stmt)) {
        checkLoop(whileNode->condition.get(), whileNode->body);
    } else if (auto forNode = dynamic_cast<ForNode*>(stmt)) {
        pushScope();
        if (forNode->init) checkStatement(forNode->init.get());
        checkLoop(forNode->condition.get(), forNode->body, forNode->update.get());
        popScope();
    } else if (auto returnNode = dynamic_cast<ReturnNode*>(stmt)) {
        const z3::expr value = returnNode->expr
            ? evalExpression(returnNode->expr.get()) : freshInt("void_return");
        // Verify @ensures postconditions at this return point
        if (current_routine) {
            verifyEnsuresAtReturn(current_routine, value);
        }
        checkLeaks(0);
        path_guard = ctx.bool_val(false);
    } else if (auto funcCall = dynamic_cast<FuncCallNode*>(stmt)) {
        // Verify @requires at this call site (statement-level function call)
        verifyRequiresAtCallSite(funcCall->name, funcCall->args);
    } else if (auto methodCall = dynamic_cast<MethodCallNode*>(stmt)) {
        // Method calls — evaluate args for bounds checking
        for (const auto& arg : methodCall->args) {
            evalExpression(arg.get());
        }
    } else if (auto assertNode = dynamic_cast<AssertNode*>(stmt)) {
        z3::expr cond = ensure_bool(evalExpression(assertNode->condition.get()), ctx);
        solver.push();
        solver.add(!cond);
        if (hasCounterexample()) {
            z3::model m = solver.get_model();
//             std::cerr << "\n[ALU CXX Z3 FATAL] Mathematical Business Logic Assertion Failed!" << std::endl;
//             std::cerr << "  Z3 Counterexample: " << m << std::endl;
            int l = current_node ? current_node->line : 1;
            int c = current_node ? current_node->col : 1;
            std::string f = current_node ? current_node->file : "";
            throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
        }
        solver.pop();
        // The condition has been proven. Add it as an assumption for the rest of the block.
        solver.add(z3::implies(path_guard, cond));
    } else if (auto freeNode = dynamic_cast<FreeNode*>(stmt)) {
        z3::expr ptr_id = evalExpression(freeNode->expr.get());
        verifyPointerValid(ptr_id, "Double Free Check (free)", true); // Requires Owned (1)
        memory_ownership = z3::store(memory_ownership, ptr_id, ctx.int_val(0)); // Freed (0)
        
        if (auto rhs_var = dynamic_cast<VarAccessNode*>(freeNode->expr.get())) {
            if (var_to_id.count(rhs_var->name)) {
                var_alive = z3::store(var_alive, ctx.int_val(var_to_id[rhs_var->name]), ctx.bool_val(false));
            }
        }
    } else if (auto derefAssign = dynamic_cast<DerefAssignNode*>(stmt)) {
        z3::expr ptr_id = evalExpression(derefAssign->ptr_expr.get());
        verifyPointerValid(ptr_id, "Dereference Assignment (Write)");
        evalExpression(derefAssign->val_expr.get());
    }
    current_node = old_node;
}

// --- Routine Checking ---

void Z3Verifier::checkRoutine(RoutineNode* node) {
    pushScope();
    current_routine = node;
    current_node = node;

    // Declare formal parameters as symbolic Z3 variables
    for (const auto& p : node->params) {
        z3::expr var = freshInt(p.name);
        declareVar(p.name, var);
    }

    // Assert @requires as ASSUMPTIONS (we assume preconditions hold within the routine body)
    auto cit = routine_contracts.find(node->name);
    if (cit != routine_contracts.end()) {
        const RoutineContract& contract = cit->second;
        
        // Build param expressions for annotation evaluation
        std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
        std::vector<ASTNode*> param_exprs;
        std::vector<z3::expr> param_values;
        for (const auto& p : contract.params) {
            param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
            param_exprs.push_back(param_exprs_mem.back().get());
            param_values.push_back(getVar(p.name));
        }

        for (ASTNode* req_expr : contract.requires_exprs) {
            if (isStringLiteralAnnotation(req_expr)) continue;
            z3::expr precondition = ensure_bool(evalAnnotationExpr(req_expr, contract.params, param_exprs, param_values), ctx);
            solver.add(precondition); // ASSUME preconditions hold inside the routine
        }
    }

    // Check all statements in the body
    for (const auto& stmt : node->body) {
        checkStatement(stmt.get());
    }

    current_routine = nullptr;
    popScope();
}

// --- Program Checking and Pruning ---

bool Z3Verifier::hasMemoryRisks(ASTNode* node) {
    if (!node) return false;
    
    // Nodes that explicitly require Z3 tracking
    if (dynamic_cast<ArrayIndexNode*>(node) ||
        dynamic_cast<ArrayAssignNode*>(node) ||
        dynamic_cast<NewAllocationNode*>(node) ||
        dynamic_cast<FreeNode*>(node) ||
        dynamic_cast<DereferenceNode*>(node) ||
        dynamic_cast<DerefAssignNode*>(node) ||
        dynamic_cast<AssertNode*>(node)) {
        return true;
    }

    // Check variables typed as pointers
    if (auto varDecl = dynamic_cast<VarDeclNode*>(node)) {
        if (!varDecl->varType.empty() && varDecl->varType.back() == '*') return true;
        if (varDecl->initializer && hasMemoryRisks(varDecl->initializer.get())) return true;
        return false;
    }

    // Check routines with contracts
    if (auto routine = dynamic_cast<RoutineNode*>(node)) {
        if (!routine->requires_annotations.empty() || !routine->ensures_annotations.empty()) return true;
        for (const auto& stmt : routine->body) {
            if (hasMemoryRisks(stmt.get())) return true;
        }
        return false;
    }

    // Recursively check children based on node type
    if (auto ifNode = dynamic_cast<IfNode*>(node)) {
        if (hasMemoryRisks(ifNode->condition.get())) return true;
        for (const auto& s : ifNode->then_body) if (hasMemoryRisks(s.get())) return true;
        for (const auto& s : ifNode->else_body) if (hasMemoryRisks(s.get())) return true;
        return false;
    } else if (auto whileNode = dynamic_cast<WhileNode*>(node)) {
        if (hasMemoryRisks(whileNode->condition.get())) return true;
        for (const auto& s : whileNode->body) if (hasMemoryRisks(s.get())) return true;
        return false;
    } else if (auto forNode = dynamic_cast<ForNode*>(node)) {
        if (forNode->init && hasMemoryRisks(forNode->init.get())) return true;
        if (forNode->condition && hasMemoryRisks(forNode->condition.get())) return true;
        if (forNode->update && hasMemoryRisks(forNode->update.get())) return true;
        for (const auto& s : forNode->body) if (hasMemoryRisks(s.get())) return true;
        return false;
    } else if (auto binop = dynamic_cast<BinOpNode*>(node)) {
        if (binop->op == "/") return true; // Division by zero check needed
        return hasMemoryRisks(binop->left.get()) || hasMemoryRisks(binop->right.get());
    } else if (auto funcCall = dynamic_cast<FuncCallNode*>(node)) {
        return true; // We need to evaluate @requires constraints of the called function
    } else if (auto methodCall = dynamic_cast<MethodCallNode*>(node)) {
        return true; 
    } else if (auto retNode = dynamic_cast<ReturnNode*>(node)) {
        if (retNode->expr) return hasMemoryRisks(retNode->expr.get());
        return false;
    } else if (auto varAssign = dynamic_cast<VarAssignNode*>(node)) {
        return hasMemoryRisks(varAssign->expr.get());
    } else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(node)) {
        return hasMemoryRisks(memberAssign->expr.get());
    } else if (auto arrDecl = dynamic_cast<ArrayDeclNode*>(node)) {
        return true;
    }

    return false;
}

void Z3Verifier::checkProgram(ProgramNode* node) {
    std::vector<std::future<void>> futures;
    std::atomic<int> skipped_count{0};
    std::mutex cerr_mutex;

    for (const auto& decl : node->declarations) {
        if (auto routine = dynamic_cast<RoutineNode*>(decl.get())) {
            if (!hasMemoryRisks(routine)) {
                skipped_count++;
                continue;
            }

            futures.push_back(std::async(std::launch::async, [this, routine, &cerr_mutex]() {
                try {
                    Z3Verifier worker;
                    worker.setContracts(this->routine_contracts);
                    worker.checkRoutinePublic(routine);
                } catch (...) {
                    std::lock_guard<std::mutex> lock(cerr_mutex);
                    // Let the exception propagate to be caught by future.get()
                    throw;
                }
            }));
        }
    }

    // Wait for all routines to complete and propagate any thrown exceptions
    for (auto& f : futures) {
        f.get();
    }

    if (skipped_count > 0) {
        std::cerr << "[ALU CXX] Z3 Pruning: Skipped " << skipped_count << " routine(s) with no memory risks." << std::endl;
    }
}

// --- Entry Point ---

void Z3Verifier::verify(ProgramNode* ast) {
    std::cerr << "[ALU CXX] Running Z3 Theorem Prover Bounds Verification..." << std::endl;

    // First pass: collect all @requires/@ensures contracts
    registerContracts(ast);

    if (!routine_contracts.empty()) {
        std::cerr << "[ALU CXX] Z3 registered " << routine_contracts.size() 
                  << " routine contract(s) for verification." << std::endl;
    }

    // Second pass: verify bounds and contracts
    checkProgram(ast);

    std::cerr << "[ALU CXX] Z3 Verification Passed: checked obligations discharged within the verifier's current model." << std::endl;
}

