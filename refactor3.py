import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# Fix declareSymbol calls without unit
# Old: declareSymbol(name, type, line, col, file)
# New: declareSymbol(name, type, "", line, col, file)

# Example: declareSymbol(decl->name, expectedType, decl->line, decl->col, decl->file);
content = re.sub(r'declareSymbol\(([^,]+),\s*([^,]+),\s*(.+?->line|line),\s*(.+?->col|col),\s*(.+?->file|file)\);', r'declareSymbol(\1, \2, "", \3, \4, \5);', content)
content = re.sub(r'declareSymbol\(p\.name, ptype, routine->line, routine->col, routine->file\);', r'declareSymbol(p.name, ptype, "", routine->line, routine->col, routine->file);', content)
content = re.sub(r'declareSymbol\(p\.name, ptype, ext->line, ext->col, ext->file\);', r'declareSymbol(p.name, ptype, "", ext->line, ext->col, ext->file);', content)

# Fix returnType = checkExpression
content = re.sub(r'returnType = checkExpression\(returnNode->expr\.get\(\)\);', r'TypeInfo returnType_info = checkExpression(returnNode->expr.get());\n            returnType = returnType_info.type;', content)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print('Refactor 3 finished.')
