#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <sstream>

class LLVMCodeGen {
private:
    std::stringstream ir_output;
    int tmp_counter;
    int label_counter = 0;
    std::unordered_map<std::string, std::string> struct_field_types; // StructName.FieldName -> LLVM type
    std::unordered_map<std::string, int> struct_field_indices; // StructName.FieldName -> index
    std::unordered_map<std::string, std::string> var_struct_type; // Local var -> StructName
    std::unordered_map<std::string, std::string> llvm_var_types; // Local var -> LLVM type string
    
public:
    LLVMCodeGen();
    
    // Core methods to retrieve the emitted IR string
    std::string getIR() const;
    void saveToFile(const std::string& filename) const;
    
    // Utility to get a new temporary register (e.g., %1, %2)
    std::string getTempReg();
    
    // Utility to get a new label (e.g., if.then1)
    std::string getLabel(const std::string& prefix);

    // Evaluate expression and return register/value
    std::string evaluateExpression(ASTNode* expr);

    // The methods that emit IR text for specific AST nodes
    void visit(AsmCallNode* node);
    void visit(UnsafeBlockNode* node);
    std::string visit(LiteralNode* node); // returns the LLVM value representation (e.g., "5")
    std::string visit(BinOpNode* node);   // returns the register where the result is stored
    void visit(VarDeclNode* node);
    void visit(VarAssignNode* node);
    void visit(IfNode* node);
    void visit(WhileNode* node);
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
    void visit(FreeNode* node);
    void visit(ArrayDeclNode* node);
    std::string visit(ArrayIndexNode* node);
    void visit(ArrayAssignNode* node);
    
    void visit(ProgramNode* node);
    
    // Direct string emission
    void emit(const std::string& code);
};
