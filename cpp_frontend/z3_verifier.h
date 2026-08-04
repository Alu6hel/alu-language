#pragma once
#include "ast.h"
#include <z3++.h>
#include <unordered_map>
#include <vector>
#include <string>

class Z3Verifier {
private:
    z3::context ctx;
    z3::solver solver;
    
    // Maps variable names to their Z3 expressions
    std::vector<std::unordered_map<std::string, z3::expr>> scope_stack;
    
    // Maps array/pointer names to their allocated size constraints
    std::unordered_map<std::string, z3::expr> bounds_table;

    void pushScope();
    void popScope();
    void declareVar(const std::string& name, const z3::expr& val);
    z3::expr getVar(const std::string& name);
    
    // AST Traversal
    void checkProgram(ProgramNode* node);
    void checkRoutine(RoutineNode* node);
    void checkStatement(ASTNode* stmt);
    z3::expr evalExpression(ASTNode* expr);

    // Bounds checking
    void verifyBounds(ASTNode* arrayExpr, ASTNode* indexExpr);

public:
    Z3Verifier();
    void verify(ProgramNode* ast);
};
