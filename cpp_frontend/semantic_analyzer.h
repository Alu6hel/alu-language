#pragma once
#include "ast.h"
#include <unordered_map>
#include <string>
#include <vector>

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
    // Scope stacks: each element is one scope level (innermost is back())
    std::vector<std::unordered_map<std::string, DataType>> scope_stack;
    std::vector<std::unordered_map<std::string, std::string>> struct_var_stack;
    std::unordered_map<std::string, std::string> pointer_var_table; 
    std::unordered_map<std::string, std::string> array_var_table; 
    std::unordered_map<std::string, FunctionSignature> function_table;
    std::unordered_map<std::string, StructInfo> struct_table;
    std::unordered_map<std::string, StructDefNode*> struct_templates;
    std::vector<std::string> current_namespace;
    
    std::string resolveName(const std::string& name);
    std::string prefixName(const std::string& name);

    ProgramNode* current_ast;
    DataType current_routine_return_type;
    
    // Scope management
    void pushScope();
    void popScope();
    
    // Scope-aware lookups (search from innermost to outermost)
    bool lookupSymbol(const std::string& name, DataType& outType);
    bool lookupStructVar(const std::string& name, std::string& outStructName);
    bool isDeclaredInCurrentScope(const std::string& name);
    void declareSymbol(const std::string& name, DataType type);
    void declareStructVar(const std::string& name, const std::string& structName);
    
    void instantiateTemplateIfNeeded(const std::string& typeStr);
    
    DataType parseDataType(const std::string& typeStr);
    DataType checkExpression(ASTNode* expr);
    void checkVarDecl(VarDeclNode* decl);
    void checkStatement(ASTNode* stmt);
    void checkRoutine(RoutineNode* routine);
    void checkProgram(ProgramNode* node);
    void checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    
public:
    void analyze(ProgramNode* ast);
};
