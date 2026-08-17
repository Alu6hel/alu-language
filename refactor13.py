import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# Update checkVarDecl
var_decl_orig = '''void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {
    decl->varType = resolveName(decl->varType);
    DataType expectedType = parseDataType(decl->varType);
    std::string declared_unit = extractUnit(decl->varType);'''

var_decl_new = '''void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {
    decl->varType = resolveName(decl->varType);
    DataType expectedType = parseDataType(decl->varType);
    std::string declared_unit = extractUnit(decl->varType);
    
    if (decl->refinement_expr) {
        pushScope();
        declareSymbol(decl->refinement_var, expectedType, declared_unit, decl->line, decl->col, decl->file);
        TypeInfo ref_type = checkExpression(decl->refinement_expr.get());
        if (ref_type.type != DataType::BOOL) {
            throw std::runtime_error("Semantic Error: Refinement expression must evaluate to a boolean");
        }
        popScope();
    }'''

content = content.replace(var_decl_orig, var_decl_new)


# Update checkRoutine parameters
routine_orig = '''for (auto& p : routine->params) {
        p.type = resolveName(p.type);
        DataType t = parseDataType(p.type);
        sig.paramTypes.push_back(t);
    }'''

routine_new = '''for (auto& p : routine->params) {
        p.type = resolveName(p.type);
        DataType t = parseDataType(p.type);
        sig.paramTypes.push_back(t);
        
        if (p.refinement_expr) {
            pushScope();
            declareSymbol(p.refinement_var, t, extractUnit(p.type), 0, 0, "");
            TypeInfo ref_type = checkExpression(p.refinement_expr.get());
            if (ref_type.type != DataType::BOOL) {
                throw std::runtime_error("Semantic Error: Refinement expression must evaluate to a boolean");
            }
            popScope();
        }
    }'''

content = content.replace(routine_orig, routine_new)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("Updated SemanticAnalyzer")
