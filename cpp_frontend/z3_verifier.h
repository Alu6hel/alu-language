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
    std::vector<std::unordered_map<std::string, int>> scope_var_ids;
    std::vector<std::unordered_map<std::string, int>> saved_var_ids;
    std::vector<std::unordered_map<std::string, z3::expr>> saved_bounds;
    int next_var_id = 1;
    int next_alloc_id = 1;
    size_t loop_steps = 0;

    // Models the heap's memory state mapping Pointer IDs (Int) to State (Int)
    // 0 = Invalid/Freed, 1 = Owned, 3 = Borrowed
    z3::expr memory_ownership;

    // Models variable liveness mapping Var ID (Int) to Alive (Bool)
    // Used to track Move Semantics (if false, the variable was moved or freed)
    z3::expr var_alive;
    z3::expr path_guard;

    struct FlowState {
        std::vector<std::unordered_map<std::string, z3::expr>> variables;
        std::vector<std::unordered_set<std::string>> owners;
        z3::expr heap;
        z3::expr liveness;
        z3::expr path;
    };
    FlowState saveFlow() const;
    void restoreFlow(const FlowState& state);
    void mergeFlow(const z3::expr& condition, const FlowState& then_state,
                   const FlowState& else_state);

    // Maps routine names to their contracts (populated during registration pass)
    std::unordered_map<std::string, RoutineContract> routine_contracts;

    // Tracks whether we are currently inside a routine with @ensures contracts
    RoutineNode* current_routine = nullptr;
    ASTNode* current_node = nullptr;

    void pushScope();
    void popScope();
    void checkLeaks(size_t first_scope);
    void declareVar(const std::string& name, const z3::expr& val);
    void assignVar(const std::string& name, const z3::expr& val);
    z3::expr getVar(const std::string& name);
    bool hasCounterexample();
    z3::expr freshInt(const std::string& prefix);
    
    // AST Traversal
    void registerContracts(ProgramNode* node);
    void registerContractsInDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations);
    void checkProgram(ProgramNode* node);
    void checkRoutine(RoutineNode* node);
    void checkStatement(ASTNode* stmt);
    void checkLoop(ASTNode* condition, const std::vector<std::unique_ptr<ASTNode>>& body,
                   ASTNode* update = nullptr);
    z3::expr evalExpression(ASTNode* expr);

    // Bounds and math checking
    void verifyBounds(ASTNode* arrayExpr, ASTNode* indexExpr);
    void verifyDivisionByZero(const z3::expr& denominator);
    void verifyPointerValid(const z3::expr& ptrExpr, const std::string& contextMsg, bool require_ownership = false);

    // Contract verification
    void verifyRequiresAtCallSite(const std::string& calleeName,
                                  const std::vector<std::unique_ptr<ASTNode>>& actual_args);
    void verifyEnsuresAtReturn(RoutineNode* routine, const z3::expr& ret_val);
    z3::expr evalAnnotationExpr(ASTNode* expr, const std::vector<Parameter>& formal_params, const std::vector<ASTNode*>& actual_args, const std::vector<z3::expr>& actual_values);
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
