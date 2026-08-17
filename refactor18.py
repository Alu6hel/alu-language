import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

pattern = re.compile(
    r'z3::expr Z3Verifier::evalAnnotationExpr\(ASTNode\* expr,\s*const std::vector<Parameter>& formal_params,\s*const std::vector<z3::expr>& actual_exprs\)\s*\{.*?\n\}',
    re.DOTALL
)

new_func = '''z3::expr Z3Verifier::evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args) {
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

content = pattern.sub(new_func, content)
with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Updated evalAnnotationExpr")
