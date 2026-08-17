#pragma once
#include "ast.h"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// Stores the contract annotations for a routine
struct RoutineContract {
    std::string name;
    std::vector<ASTNode*> requires_exprs;  // @requires annotation AST expressions
    std::vector<ASTNode*> ensures_exprs;   // @ensures annotation AST expressions
    std::vector<Parameter> params;         // formal parameter list
    std::string returnType;
};

class Z3Verifier {
private:
    z3::context ctx;
    z3::solver solver;
    
    // Maps variable names to their Z3 expressions
    std::vector<std::unordered_map<std::string, z3::expr>> scope_stack;
    
    // Maps array/pointer names to their allocated size constraints
    std::unordered_map<std::string, z3::expr> bounds_table;

    // Tracks which variables in the current scope are owned pointers (for leak detection)
    std::vector<std::unordered_set<std::string>> owned_pointers_stack;

    // Maps variable names to unique integer IDs for var_alive tracking
    std::unordered_map<std::string, int> var_to_id;
    int next_var_id = 1;

    // Models the heap's memory state mapping Pointer IDs (Int) to State (Int)
    // 0 = Invalid/Freed, 1 = Owned, 3 = Borrowed
    z3::expr memory_ownership;

    // Models variable liveness mapping Var ID (Int) to Alive (Bool)
    // Used to track Move Semantics (if false, the variable was moved or freed)
    z3::expr var_alive;

    // Maps routine names to their contracts (populated during registration pass)
    std::unordered_map<std::string, RoutineContract> routine_contracts;

    // Tracks whether we are currently inside a routine with @ensures contracts
    RoutineNode* current_routine = nullptr;
    ASTNode* current_node = nullptr;

    void pushScope();
    void popScope();
    void declareVar(const std::string& name, const z3::expr& val);
    z3::expr getVar(const std::string& name);
    
    // AST Traversal
    void registerContracts(ProgramNode* node);
    void registerContractsInDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkProgram(ProgramNode* node);
    void checkRoutine(RoutineNode* node);
    void checkStatement(ASTNode* stmt);
    z3::expr evalExpression(ASTNode* expr);

    // Bounds and math checking
    void verifyBounds(ASTNode* arrayExpr, ASTNode* indexExpr);
    void verifyDivisionByZero(const z3::expr& denominator);
    void verifyPointerValid(const z3::expr& ptrExpr, const std::string& contextMsg, bool require_ownership = false);

    // Contract verification
    void verifyRequiresAtCallSite(const std::string& calleeName,
                                  const std::vector<std::unique_ptr<ASTNode>>& actual_args);
    void verifyEnsuresAtReturn(RoutineNode* routine, ASTNode* returnExpr);
    z3::expr evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args);
    bool isStringLiteralAnnotation(ASTNode* expr);

public:
    Z3Verifier();
    void verify(ProgramNode* ast);
    
    // For multithreaded parallel verification
    void setContracts(const std::unordered_map<std::string, RoutineContract>& contracts) {
        this->routine_contracts = contracts;
    }
    void checkRoutinePublic(RoutineNode* node) { checkRoutine(node); }
    
    // Pruning strategy
    static bool hasMemoryRisks(ASTNode* node);
};
