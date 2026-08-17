import re

with open('cpp_frontend/z3_verifier.h', 'r') as f:
    content = f.read()

pattern = re.compile(
    r'z3::expr evalAnnotationExpr\(ASTNode\* expr,\s*const std::vector<Parameter>& formal_params,\s*const std::vector<z3::expr>& actual_exprs\);',
    re.DOTALL
)

new_func = 'z3::expr evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args);'

content = pattern.sub(new_func, content)
with open('cpp_frontend/z3_verifier.h', 'w') as f:
    f.write(content)

print("Updated evalAnnotationExpr header")
