#pragma once
#include "ast.h"
#include <unordered_map>
#include <string>

struct FunctionSignature {
    DataType returnType;
    std::vector<DataType> paramTypes;
    bool isVariadic = false;
};

struct StructInfo {
    std::string name;
    std::vector<StructField> fields;
};

class SemanticAnalyzer {
private:
    std::unordered_map<std::string, DataType> symbol_table;
    std::unordered_map<std::string, std::string> struct_var_table; 
    std::unordered_map<std::string, std::string> pointer_var_table; 
    std::unordered_map<std::string, std::string> array_var_table; 
    std::unordered_map<std::string, FunctionSignature> function_table;
    std::unordered_map<std::string, StructInfo> struct_table;
    DataType current_routine_return_type;
    
    DataType parseDataType(const std::string& typeStr);
    DataType checkExpression(ASTNode* expr);
    void checkVarDecl(VarDeclNode* decl);
    void checkStatement(ASTNode* stmt);
    void checkRoutine(RoutineNode* routine);
    void checkProgram(ProgramNode* node);
    
public:
    void analyze(ProgramNode* ast);
};
