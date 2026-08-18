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
    std::stringstream* current_output = &ir_output;
    std::vector<std::string> allocas;
    bool block_terminated = false;
    std::stringstream global_strings_output;
    int tmp_counter;
    int label_counter = 0;
    int global_str_counter = 0;
    
    // Scope stacks for variable type tracking
    std::vector<std::map<std::string, std::string>> var_type_stack;   // varName -> LLVM Type
    std::vector<std::map<std::string, std::string>> alu_type_stack;   // varName -> Alu Type
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
    std::set<std::string> extern_functions; std::set<std::string> opaque_types; // track functions that are external C routines (no ARC)
    std::string current_func_ret_type; // track return type of current routine
    std::string current_namespace;     // track current namespace for name mangling
    std::string target_arch; // Target architecture string

    // DWARF Debug Info State

    int di_counter = 10;
    int di_cu_id = -1;
    std::map<std::string, int> di_file_ids;
    std::map<std::string, int> di_type_ids;
    std::stringstream di_output;
    ASTNode* current_debug_node = nullptr;
    int current_di_scope = -1;

    int getDIFile(const std::string& filename);
    int getDIType(const std::string& typeName);
    std::string getDILoc();


public:
    bool emit_debug_info = false;
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
    std::string lookupAluType(const std::string& name);
    std::string lookupStructType(const std::string& name);
    void declareVarType(const std::string& name, const std::string& llvmType);
    void declareAluType(const std::string& name, const std::string& aluType);
    void declareStructType(const std::string& name, const std::string& structName);
    
    // Name mangling for LLVM IR unique names
    std::string getUniqueName(const std::string& sourceName);
    std::string lookupIRName(const std::string& sourceName);
    std::string getNamespacedName(const std::string& name);
    std::string getLLVMType(const std::string& type);

    // Evaluate expression and return register/value
    std::string evaluateExpression(ASTNode* expr);
    std::string getInferredLLVMType(ASTNode* expr);

    // The methods that emit IR text for specific AST nodes
    void visit(AsmCallNode* node);
    void visit(UnsafeBlockNode* node);
    std::string visit(LiteralNode* node); // returns the LLVM value representation (e.g., "5")
    std::string visit(BinOpNode* node);   // returns the register where the result is stored
    std::string visit(VectorInitNode* node);
    void visit(VarDeclNode* node);
    void visit(VarAssignNode* node);
    void visit(IfNode* node);
    void visit(WhileNode* node);
    void visit(ForNode* node);
    void visit(TryCatchNode* node);
    void visit(ThrowNode* node);
    void visit(ReturnNode* node);
    void visit(AssertNode* node);
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
    
    // Namespace helpers
    void visit(NamespaceNode* node);
    void registerReturnTypes(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void codegenDeclarationsPrePass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void codegenDeclarationsMainPass(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    
    // Direct string emission
    void emit(const std::string& code, ASTNode* node = nullptr);
    void emitAlloca(const std::string& code);
};

