#pragma once
#include "ast.h"
#include <unordered_map>
#include <string>

class SemanticAnalyzer {
private:
    std::unordered_map<std::string, DataType> symbol_table;
    
    DataType checkExpression(ASTNode* expr);
    void checkVarDecl(VarDeclNode* decl);
    void checkStatement(ASTNode* stmt);
    void checkRoutine(RoutineNode* routine);
    
public:
    void analyze(ProgramNode* ast);
};
