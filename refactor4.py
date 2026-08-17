import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

start_idx = content.find('TypeInfo SemanticAnalyzer::checkExpression(ASTNode* expr) {')
end_idx = content.find('void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {')
check_expr_content = content[start_idx:end_idx]

lines = check_expr_content.split('\n')
new_lines = []
for line in lines:
    m = re.search(r'return\s+([^;]+);', line)
    if m:
        ret_val = m.group(1).strip()
        if not ret_val.startswith('{'):
            line = line.replace('return ' + ret_val + ';', 'return {' + ret_val + ', ""};')
    new_lines.append(line)

check_expr_content = '\n'.join(new_lines)
content = content[:start_idx] + check_expr_content + content[end_idx:]

content = re.sub(r'declareSymbol\(decl->name, expectedType, decl->line, decl->col, decl->file\);', r'declareSymbol(decl->name, expectedType, "", decl->line, decl->col, decl->file);', content)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print('Refactor 4 finished.')
