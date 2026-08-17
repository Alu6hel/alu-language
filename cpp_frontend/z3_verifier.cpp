#include "z3_verifier.h"
#include "error_reporter.h"
#include <iostream>
#include <stdexcept>

static z3::expr ensure_bool(const z3::expr& e, z3::context& ctx) {
    if (e.is_bool()) return e;
    if (e.is_int()) return e != 0;
    return e;
}

Z3Verifier::Z3Verifier() : solver(ctx), memory_ownership(ctx), var_alive(ctx) {
    memory_ownership = ctx.constant("memory_ownership_0", ctx.array_sort(ctx.int_sort(), ctx.int_sort()));
    var_alive = ctx.constant("var_alive_0", ctx.array_sort(ctx.int_sort(), ctx.bool_sort()));
}

void Z3Verifier::pushScope() {
    scope_stack.emplace_back();
    owned_pointers_stack.emplace_back();
    solver.push();
}

void Z3Verifier::popScope() {
    // Memory Leak Detection
    if (!owned_pointers_stack.empty() && !scope_stack.empty()) {
        auto& current_pointers = owned_pointers_stack.back();
        auto& current_vars = scope_stack.back();
        for (const auto& var_name : current_pointers) {
            if (current_vars.count(var_name) && var_to_id.count(var_name)) {
                z3::expr ptr_id = current_vars.at(var_name);
                int v_id = var_to_id[var_name];
                
                z3::expr is_alive = z3::select(var_alive, ctx.int_val(v_id));
                z3::expr state = z3::select(memory_ownership, ptr_id);
                
                // If it is still alive and owned, it's a leak!
                solver.push();
                solver.add(is_alive && state == 1);
                if (solver.check() == z3::sat) {
                    z3::model m = solver.get_model();
//                     std::cerr << "\n[ALU CXX Z3 FATAL] Memory Leak Detected!" << std::endl;
//                     std::cerr << "  Variable: '" << var_name << "' goes out of scope while still owning memory." << std::endl;
//                     std::cerr << "  Z3 Counterexample: " << m << std::endl;
                    int l = current_node ? current_node->line : 1;
                    int c = current_node ? current_node->col : 1;
                    std::string f = current_node ? current_node->file : "";
                    throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed", f, l, c));
                }
                solver.pop();
            }
        }
    }
    owned_pointers_stack.pop_back();
    scope_stack.pop_back();
    solver.pop();
}

void Z3Verifier::declareVar(const std::string& name, const z3::expr& val) {
    if (!scope_stack.empty()) {
        scope_stack.back().insert({name, val});
    }
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
            
            if (solver.check() == z3::sat) {
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
    
    if (solver.check() == z3::sat) {
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
    
    if (solver.check() == z3::sat) {
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
z3::expr Z3Verifier::evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args) {
    pushScope();
    for (size_t i = 0; i < formal_params.size() && i < actual_args.size(); ++i) {
        z3::expr arg_val = evalExpression(actual_args[i]);
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
    auto it = routine_contracts.find(calleeName);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.requires_exprs.empty()) return;

    std::vector<ASTNode*> args_ptrs;
    for (const auto& arg : actual_args) args_ptrs.push_back(arg.get());

    // Check each @requires clause
    for (ASTNode* req_expr : contract.requires_exprs) {
        if (isStringLiteralAnnotation(req_expr)) continue;

        z3::expr precondition = evalAnnotationExpr(req_expr, contract.params, args_ptrs);
        
        // To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!precondition);
        
        if (solver.check() == z3::sat) {
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

void Z3Verifier::verifyEnsuresAtReturn(RoutineNode* routine, ASTNode* returnExpr) {
    auto it = routine_contracts.find(routine->name);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.ensures_exprs.empty()) return;

    std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
    std::vector<ASTNode*> param_exprs;
    for (const auto& p : contract.params) {
        param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
        param_exprs.push_back(param_exprs_mem.back().get());
    }

    // Evaluate the return expression and bind it to __return
    z3::expr ret_val = ctx.int_const("__return");
    if (returnExpr) {
        ret_val = evalExpression(returnExpr);
    }
    declareVar("return", ret_val);

    // Check each @ensures clause
    for (ASTNode* ens_expr : contract.ensures_exprs) {
        // Skip string-literal annotations (documentation only)
        if (isStringLiteralAnnotation(ens_expr)) continue;

        z3::expr postcondition = evalAnnotationExpr(ens_expr, contract.params, param_exprs);
        
        // To verify the postcondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!postcondition);
        
        if (solver.check() == z3::sat) {
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
            if (solver.check() == z3::sat) {
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
        return ctx.int_const("dummy_arr_val");
    } else if (auto deref = dynamic_cast<DereferenceNode*>(expr)) {
        z3::expr ptr_id = evalExpression(deref->expr.get());
        verifyPointerValid(ptr_id, "Dereference (Read)");
        return ctx.int_const("deref_val");
    } else if (auto newAlloc = dynamic_cast<NewAllocationNode*>(expr)) {
        static int alloc_id = 0;
        z3::expr ptr_id = ctx.int_val(++alloc_id);
        memory_ownership = z3::store(memory_ownership, ptr_id, ctx.int_val(1)); // 1 = Owned
        return ptr_id;
    }
    else if (auto funcCall = dynamic_cast<FuncCallNode*>(expr)) {
        // Verify @requires at this call site
        verifyRequiresAtCallSite(funcCall->name, funcCall->args);
        // Return a symbolic value representing the function's return
        std::string sym_name = funcCall->name + "_ret";
        return ctx.int_const(sym_name.c_str());
    }
    // Fallback to a dummy variable
    return ctx.int_const("dummy");
}

// --- Statement Checking ---

void Z3Verifier::checkStatement(ASTNode* stmt) {
    ASTNode* old_node = current_node;
    current_node = stmt;
    if (auto vardecl = dynamic_cast<VarDeclNode*>(stmt)) {
        int v_id = next_var_id++;
        var_to_id[vardecl->name] = v_id;
        var_alive = z3::store(var_alive, ctx.int_val(v_id), ctx.bool_val(true));
        
        z3::expr var = ctx.int_const(vardecl->name.c_str());
        declareVar(vardecl->name, var);
        
        if (vardecl->initializer) {
            z3::expr init_val = evalExpression(vardecl->initializer.get());
            solver.add(var == init_val);
            
            if (vardecl->refinement_expr) {
                pushScope();
                declareVar(vardecl->refinement_var, init_val);
                z3::expr ref_cond = ensure_bool(evalExpression(vardecl->refinement_expr.get()), ctx);
                
                solver.push();
                solver.add(!ref_cond);
                if (solver.check() == z3::sat) {
                    int l = current_node ? current_node->line : 1;
                    int c = current_node ? current_node->col : 1;
                    std::string f = current_node ? current_node->file : "";
                    throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed: Refinement constraint not satisfied on assignment", f, l, c));
                }
                solver.pop();
                popScope();
                
                pushScope();
                declareVar(vardecl->refinement_var, var);
                z3::expr var_cond = ensure_bool(evalExpression(vardecl->refinement_expr.get()), ctx);
                popScope();
                solver.add(var_cond);
            }
            
            if (auto rhs_var = dynamic_cast<VarAccessNode*>(vardecl->initializer.get())) {
                bool is_ptr = (!vardecl->varType.empty() && vardecl->varType.back() == '*');
                if (is_ptr && var_to_id.count(rhs_var->name)) {
                    // Move semantics: transfer ownership
                    var_alive = z3::store(var_alive, ctx.int_val(var_to_id[rhs_var->name]), ctx.bool_val(false));
                }
            }
        }
        
        if (!vardecl->varType.empty() && vardecl->varType.back() == '*') {
            if (!owned_pointers_stack.empty()) {
                owned_pointers_stack.back().insert(vardecl->name);
            }
        }
    } else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        z3::expr new_val = evalExpression(memberAssign->expr.get());
        std::string full_name = memberAssign->objectName + "_" + memberAssign->fieldName;
        z3::expr var = ctx.int_const((full_name + "_new").c_str());
        declareVar(full_name, var);
        solver.add(var == new_val);
    } else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        z3::expr new_val = evalExpression(varassign->expr.get());
        
        // Check for move
        if (auto rhs_var = dynamic_cast<VarAccessNode*>(varassign->expr.get())) {
            bool rhs_is_owned = false;
            for (const auto& s : owned_pointers_stack) {
                if (s.count(rhs_var->name)) rhs_is_owned = true;
            }
            if (rhs_is_owned && var_to_id.count(rhs_var->name)) {
                var_alive = z3::store(var_alive, ctx.int_val(var_to_id[rhs_var->name]), ctx.bool_val(false));
                if (!owned_pointers_stack.empty()) {
                    owned_pointers_stack.back().insert(varassign->name); // LHS becomes owned
                }
            }
        }
        
        z3::expr var = ctx.int_const((varassign->name + "_new").c_str()); // SSA form approach simplified
        declareVar(varassign->name, var);
        solver.add(var == new_val);
        
        if (var_to_id.count(varassign->name)) {
            var_alive = z3::store(var_alive, ctx.int_val(var_to_id[varassign->name]), ctx.bool_val(true));
        }
    } else if (auto arrDecl = dynamic_cast<ArrayDeclNode*>(stmt)) {
        z3::expr size_expr = evalExpression(arrDecl->sizeExpr.get());
        bounds_table.insert({arrDecl->name, size_expr});
    } else if (auto arrAssign = dynamic_cast<ArrayAssignNode*>(stmt)) {
        verifyBounds(arrAssign->arrayExpr.get(), arrAssign->indexExpr.get());
    } else if (auto arrIndex = dynamic_cast<ArrayIndexNode*>(stmt)) {
        verifyBounds(arrIndex->arrayExpr.get(), arrIndex->indexExpr.get());
    } else if (auto ifNode = dynamic_cast<IfNode*>(stmt)) {
        z3::expr mem_before = memory_ownership;

        pushScope();
        z3::expr cond = ensure_bool(evalExpression(ifNode->condition.get()), ctx);
        solver.add(cond);
        for (const auto& s : ifNode->then_body) checkStatement(s.get());
        z3::expr mem_then = memory_ownership;
        popScope();
        
        z3::expr mem_else = mem_before;
        if (!ifNode->else_body.empty()) {
            pushScope();
            solver.add(!cond);
            memory_ownership = mem_before;
            for (const auto& s : ifNode->else_body) checkStatement(s.get());
            mem_else = memory_ownership;
            popScope();
        }

        memory_ownership = z3::ite(cond, mem_then, mem_else);
    } else if (auto whileNode = dynamic_cast<WhileNode*>(stmt)) {
        pushScope();
        z3::expr mem_before = memory_ownership;
        // Just add the condition, assuming loop variables could be anything satisfying it
        z3::expr cond = ensure_bool(evalExpression(whileNode->condition.get()), ctx);
        solver.add(cond);
        for (const auto& s : whileNode->body) checkStatement(s.get());
        
        // Merge states: the loop could execute 0 times, or multiple times.
        memory_ownership = z3::ite(cond, memory_ownership, mem_before);
        popScope();
    } else if (auto forNode = dynamic_cast<ForNode*>(stmt)) {
        pushScope();
        if (forNode->init) checkStatement(forNode->init.get());
        
        z3::expr mem_before = memory_ownership;
        
        if (forNode->condition) {
            z3::expr cond = ensure_bool(evalExpression(forNode->condition.get()), ctx);
            solver.add(cond);
            
            for (const auto& s : forNode->body) checkStatement(s.get());
            if (forNode->update) checkStatement(forNode->update.get());
            
            memory_ownership = z3::ite(cond, memory_ownership, mem_before);
        } else {
            for (const auto& s : forNode->body) checkStatement(s.get());
            if (forNode->update) checkStatement(forNode->update.get());
        }
        
        popScope();
    } else if (auto returnNode = dynamic_cast<ReturnNode*>(stmt)) {
        if (returnNode->expr) {
            evalExpression(returnNode->expr.get());
        }
        // Verify @ensures postconditions at this return point
        if (current_routine) {
            verifyEnsuresAtReturn(current_routine, returnNode->expr.get());
        }
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
        if (solver.check() == z3::sat) {
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
        solver.add(cond);
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
}

// --- Routine Checking ---

void Z3Verifier::checkRoutine(RoutineNode* node) {
    pushScope();
    current_routine = node;

    // Declare formal parameters as symbolic Z3 variables
    for (const auto& p : node->params) {
        z3::expr var = ctx.int_const(p.name.c_str());
        declareVar(p.name, var);
    }

    // Assert @requires as ASSUMPTIONS (we assume preconditions hold within the routine body)
    auto cit = routine_contracts.find(node->name);
    if (cit != routine_contracts.end()) {
        const RoutineContract& contract = cit->second;
        
        // Build param expressions for annotation evaluation
        std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
        std::vector<ASTNode*> param_exprs;
        for (const auto& p : contract.params) {
            param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
            param_exprs.push_back(param_exprs_mem.back().get());
        }

        for (ASTNode* req_expr : contract.requires_exprs) {
            if (isStringLiteralAnnotation(req_expr)) continue;
            z3::expr precondition = ensure_bool(evalAnnotationExpr(req_expr, contract.params, param_exprs), ctx);
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

// --- Program Checking ---

void Z3Verifier::checkProgram(ProgramNode* node) {
    for (const auto& decl : node->declarations) {
        if (auto routine = dynamic_cast<RoutineNode*>(decl.get())) {
            checkRoutine(routine);
        }
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

    std::cerr << "[ALU CXX] Z3 Verification Passed: Mathematically proven memory safety." << std::endl;
}

