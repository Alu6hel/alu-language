#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <unordered_map>
#include <set>

class LLVMCodeGen {
private:
    std::stringstream ir_output;
    std::stringstream global_strings_output;
    int tmp_counter;
    int label_counter = 0;
    int global_str_counter = 0;
    
    // Scope stacks for variable type tracking
    std::vector<std::map<std::string, std::string>> var_type_stack;   // varName -> LLVM Type
    std::vector<std::map<std::string, std::string>> struct_type_stack; // varName -> Struct Name
    
    // Exception handling state
    std::vector<std::pair<std::string, int>> catch_labels;
    
    // Name mangling: source name -> unique LLVM IR name (per scope level)
    std::vector<std::map<std::string, std::string>> ir_name_stack;
    std::map<std::string, int> name_counter; // tracks how many times each name has been used in a function
    
    std::map<std::string, std::string> struct_field_types; // map StructName.fieldName -> LLVM Type
    std::map<std::string, int> struct_field_indices; // map StructName.fieldName -> index
    std::map<std::string, std::string> func_return_types; // map funcName -> LLVM Type
    std::map<std::string, std::string> func_signatures; // map funcName -> signature string (if varargs)
    std::set<std::string> extern_functions; // track functions that are external C routines (no ARC)
    std::string current_func_ret_type; // track return type of current routine
    std::string target_arch; // Target architecture string

public:
    LLVMCodeGen(const std::string& target = "");
    
    // Core methods to retrieve the emitted IR string
    std::string getIR() const;
    void saveToFile(const std::string& filename) const;
    void emitUnhandledFallback();

    
    // Utility to get a new temporary register (e.g., %1, %2)
    std::string getTempReg();
    
    // Utility to get a new label (e.g., if.then1)
    std::string getLabel(const std::string& prefix);

    // Scope management
    void pushScope();
    void popScope();
    void emitScopeReleases(int levels = -1);
    void emitExceptionUnwind();
    std::string lookupVarType(const std::string& name);
    std::string lookupStructType(const std::string& name);
    void declareVarType(const std::string& name, const std::string& llvmType);
    void declareStructType(const std::string& name, const std::string& structName);
    
    // Name mangling for LLVM IR unique names
    std::string getUniqueName(const std::string& sourceName);
    std::string lookupIRName(const std::string& sourceName);

    // Evaluate expression and return register/value
    std::string evaluateExpression(ASTNode* expr);
    std::string getInferredLLVMType(ASTNode* expr);

    // The methods that emit IR text for specific AST nodes
    void visit(AsmCallNode* node);
    void visit(UnsafeBlockNode* node);
    std::string visit(LiteralNode* node); // returns the LLVM value representation (e.g., "5")
    std::string visit(BinOpNode* node);   // returns the register where the result is stored
    void visit(VarDeclNode* node);
    void visit(VarAssignNode* node);
    void visit(IfNode* node);
    void visit(WhileNode* node);
    void visit(ForNode* node);
    void visit(TryCatchNode* node);
    void visit(ThrowNode* node);
    void visit(ReturnNode* node);
    std::string visit(FuncCallNode* node);
    void visit(RoutineNode* node);
    void visit(ExternRoutineNode* node);
    void visit(StructDefNode* node);
    std::string visit(MemberAccessNode* node);
    void visit(MemberAssignNode* node);
    
    // Pointers & Arrays
    std::string visit(VarAccessNode* node);
    std::string visit(AddressOfNode* node);
    std::string visit(DereferenceNode* node);
    void visit(DerefAssignNode* node);
    std::string visit(NewAllocationNode* node);
    std::string visit(CastNode* node);
    void visit(FreeNode* node);
    void visit(ArrayDeclNode* node);
    std::string visit(ArrayIndexNode* node);
    void visit(ArrayAssignNode* node);
    std::string visit(MethodCallNode* node);
    void visit(ImportNode* node);
    
    void visit(EffectDeclNode* node);
    void visit(HandleNode* node);
    void visit(YieldNode* node);
    void visit(ResumeNode* node);
    void visit(ProgramNode* node);
    
    // Direct string emission
    void emit(const std::string& code);
};

