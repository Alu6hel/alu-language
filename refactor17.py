import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

req_orig = '''void Z3Verifier::verifyRequiresAtCallSite(const std::string& calleeName,
                                           const std::vector<std::unique_ptr<ASTNode>>& actual_args) {
    auto it = routine_contracts.find(calleeName);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.requires_exprs.empty()) return;

    // Evaluate actual arguments into Z3 expressions
    std::vector<z3::expr> actual_z3;
    for (const auto& arg : actual_args) {
        actual_z3.push_back(evalExpression(arg.get()));
    }

    // Check each @requires clause
    for (ASTNode* req_expr : contract.requires_exprs) {
        // Skip string-literal annotations (documentation only)
        if (isStringLiteralAnnotation(req_expr)) continue;

        z3::expr precondition = evalAnnotationExpr(req_expr, contract.params, actual_z3);'''

req_new = '''void Z3Verifier::verifyRequiresAtCallSite(const std::string& calleeName,
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

        z3::expr precondition = evalAnnotationExpr(req_expr, contract.params, args_ptrs);'''

content = content.replace(req_orig, req_new)

eval_orig = '''z3::expr Z3Verifier::evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<z3::expr>& actual_exprs) {
    pushScope();
    for (size_t i = 0; i < formal_params.size() && i < actual_exprs.size(); ++i) {
        declareVar(formal_params[i].name, actual_exprs[i]);
        if (!formal_params[i].refinement_var.empty()) {
            declareVar(formal_params[i].refinement_var, actual_exprs[i]);
        }
    }
    z3::expr result = ensure_bool(evalExpression(expr), ctx);
    popScope();
    return result;
}'''

eval_new = '''z3::expr Z3Verifier::evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args) {
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
}'''

content = content.replace(eval_orig, eval_new)

with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

with open('cpp_frontend/z3_verifier.h', 'r') as f:
    hcontent = f.read()

hcontent = hcontent.replace('z3::expr evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<z3::expr>& actual_exprs);',
                            'z3::expr evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args);')
with open('cpp_frontend/z3_verifier.h', 'w') as f:
    f.write(hcontent)

print("Updated Z3Verifier for param mapping")
