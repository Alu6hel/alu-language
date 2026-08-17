#pragma once
#include "ast.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

struct FunctionSignature {
    DataType returnType;
    std::vector<DataType> paramTypes;
    bool isVariadic = false;
    int def_line = 0;
    int def_col = 0;
    std::string def_file = "";
};

struct StructInfo {
    std::string name;
    std::vector<StructField> fields;
};

struct LSPSymbol {
    int line, col, length;
    std::string name;
    std::string hover_text;
    std::string def_file;
    int def_line;
    int def_col;
};

struct SymbolMeta {
    DataType type;
    std::string unit = "";
    int def_line = 0;
    int def_col = 0;
    std::string def_file = "";
};

struct TypeInfo {
    DataType type;
    std::string unit;
};


class SemanticAnalyzer {
public:
    std::vector<LSPSymbol> lsp_symbols;
    bool is_lsp_mode = false;

private:
    // Scope stacks: each element is one scope level (innermost is back())
    std::vector<std::unordered_map<std::string, SymbolMeta>> scope_stack;
    std::vector<std::unordered_map<std::string, std::string>> struct_var_stack;
    std::unordered_map<std::string, std::string> pointer_var_table; 
    std::unordered_map<std::string, std::string> array_var_table; 
    std::unordered_map<std::string, FunctionSignature> function_table;
    std::unordered_map<std::string, StructInfo> struct_table;
    std::unordered_map<std::string, StructDefNode*> struct_templates;
    std::unordered_map<std::string, RoutineNode*> routine_templates;
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
    bool lookupSymbolMeta(const std::string& name, SymbolMeta& outMeta);
    bool lookupStructVar(const std::string& name, std::string& outStructName);
    bool isDeclaredInCurrentScope(const std::string& name);
    void declareSymbol(const std::string& name, DataType type, const std::string& unit = "", int line = 0, int col = 0, const std::string& file = "");
    void declareStructVar(const std::string& name, const std::string& structName);
    
    void instantiateTemplateIfNeeded(const std::string& typeStr);
    void instantiateRoutineTemplateIfNeeded(const std::string& name, const std::vector<std::string>& type_args);
    
    DataType parseDataType(const std::string& typeStr);
    TypeInfo checkExpression(ASTNode* expr);
    void checkVarDecl(VarDeclNode* decl);
    void checkStatement(ASTNode* stmt);
    void checkRoutine(RoutineNode* routine);
    void checkProgram(ProgramNode* node);
    void checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    
    void analyzeOwnership();
    bool detectCycle(const std::string& current, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& recStack, std::vector<std::string>& path, std::unordered_map<std::string, std::vector<std::string>>& adj);
    
public:
    void analyze(ProgramNode* ast);
};
