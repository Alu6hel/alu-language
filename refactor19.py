import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

# checkRoutine
routine_orig = '''// Build param expressions for annotation evaluation
        std::vector<z3::expr> param_exprs;
        for (const auto& p : contract.params) {
            param_exprs.push_back(getVar(p.name));
        }

        for (ASTNode* req_expr : contract.requires_exprs) {
            if (isStringLiteralAnnotation(req_expr)) continue;
            z3::expr precondition = ensure_bool(evalAnnotationExpr(req_expr, contract.params, param_exprs), ctx);
            solver.add(precondition); // ASSUME preconditions hold inside the routine
        }'''

routine_new = '''// Build param expressions for annotation evaluation
        std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
        std::vector<ASTNode*> param_exprs;
        for (const auto& p : contract.params) {
            param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
            param_exprs.push_back(param_exprs_mem.back().get());
        }

        for (ASTNode* req_expr : contract.requires_exprs) {
            if (isStringLiteralAnnotation(req_expr)) continue;
            z3::expr precondition = ensure_bool(evalAnnotationExpr(req_expr, contract.params, param_exprs), ctx);
            solver.add(precondition); // ASSUME preconditions hold inside the routine
        }'''

content = content.replace(routine_orig, routine_new)

# verifyEnsuresAtReturn
ens_orig = '''void Z3Verifier::verifyEnsuresAtReturn(RoutineNode* current_routine, ASTNode* return_expr) {
    auto it = routine_contracts.find(current_routine->name);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.ensures_exprs.empty()) return;

    // Bind 'return' variable
    if (return_expr) {
        z3::expr ret_val = evalExpression(return_expr);
        declareVar("return", ret_val);
    }

    std::vector<z3::expr> param_exprs;
    for (const auto& p : current_routine->params) {
        param_exprs.push_back(getVar(p.name));
    }

    for (ASTNode* ens_expr : contract.ensures_exprs) {
        if (isStringLiteralAnnotation(ens_expr)) continue;

        z3::expr postcondition = evalAnnotationExpr(ens_expr, contract.params, param_exprs);'''

ens_new = '''void Z3Verifier::verifyEnsuresAtReturn(RoutineNode* current_routine, ASTNode* return_expr) {
    auto it = routine_contracts.find(current_routine->name);
    if (it == routine_contracts.end()) return;

    const RoutineContract& contract = it->second;
    if (contract.ensures_exprs.empty()) return;

    // Bind 'return' variable
    if (return_expr) {
        z3::expr ret_val = evalExpression(return_expr);
        declareVar("return", ret_val);
    }

    std::vector<std::unique_ptr<ASTNode>> param_exprs_mem;
    std::vector<ASTNode*> param_exprs;
    for (const auto& p : current_routine->params) {
        param_exprs_mem.push_back(std::make_unique<VarAccessNode>(p.name));
        param_exprs.push_back(param_exprs_mem.back().get());
    }

    for (ASTNode* ens_expr : contract.ensures_exprs) {
        if (isStringLiteralAnnotation(ens_expr)) continue;

        z3::expr postcondition = evalAnnotationExpr(ens_expr, contract.params, param_exprs);'''

content = content.replace(ens_orig, ens_new)

with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Updated checkRoutine and verifyEnsuresAtReturn")
