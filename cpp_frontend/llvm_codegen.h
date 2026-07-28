#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <sstream>

class LLVMCodeGen {
private:
    std::stringstream ir_output;
    int tmp_counter;
    
public:
    LLVMCodeGen();
    
    // Core methods to retrieve the emitted IR string
    std::string getIR() const;
    void saveToFile(const std::string& filename) const;
    
    // Utility to get a new temporary register (e.g., %1, %2)
    std::string getTempReg();

    // The methods that emit IR text for specific AST nodes
    void visit(AsmCallNode* node);
    void visit(UnsafeBlockNode* node);
    std::string visit(LiteralNode* node); // returns the LLVM value representation (e.g., "5")
    std::string visit(BinOpNode* node);   // returns the register where the result is stored
    void visit(VarDeclNode* node);
    void visit(RoutineNode* node);
    void visit(ProgramNode* node);
    
    // Direct string emission
    void emit(const std::string& code);
};
