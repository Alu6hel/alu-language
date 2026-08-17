import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# Replace definition
content = content.replace('DataType SemanticAnalyzer::checkExpression(ASTNode* expr) {', 'TypeInfo SemanticAnalyzer::checkExpression(ASTNode* expr) {')

# Replace all occurrences of DataType var = checkExpression(...)
# with TypeInfo var##_info = checkExpression(...); DataType var = var##_info.type;
def replacer(match):
    var_name = match.group(1)
    expr = match.group(2)
    return f"TypeInfo {var_name}_info = checkExpression({expr});\n        DataType {var_name} = {var_name}_info.type;"

content = re.sub(r'DataType\s+(\w+)\s*=\s*checkExpression\((.*?)\);', replacer, content)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print('Refactor script finished.')
