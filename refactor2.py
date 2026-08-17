import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# I will find the bounds of checkExpression
start_idx = content.find('TypeInfo SemanticAnalyzer::checkExpression(ASTNode* expr) {')
end_idx = content.find('void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {')

check_expr_content = content[start_idx:end_idx]

# Replace return types inside checkExpression
check_expr_content = re.sub(r'return (DataType::\w+);', r'return {\1, ""};', check_expr_content)
check_expr_content = re.sub(r'return t;', r'return {t, ""};', check_expr_content)
check_expr_content = re.sub(r'return leftT;', r'return {leftT, leftT_info.unit};', check_expr_content)
check_expr_content = re.sub(r'return rightT;', r'return {rightT, rightT_info.unit};', check_expr_content)
check_expr_content = re.sub(r'return actualType;', r'return {actualType, ""};', check_expr_content)
check_expr_content = re.sub(r'return exprType;', r'return {exprType, ""};', check_expr_content)
check_expr_content = re.sub(r'return valType;', r'return {valType, ""};', check_expr_content)
check_expr_content = re.sub(r'return objType;', r'return {objType, ""};', check_expr_content)

content = content[:start_idx] + check_expr_content + content[end_idx:]

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print('Refactor 2 finished.')
