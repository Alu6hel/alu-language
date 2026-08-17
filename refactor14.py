import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

# Update checkStatement for VarDeclNode
stmt_orig = '''if (vardecl->initializer) {
            z3::expr init_val = evalExpression(vardecl->initializer.get());
            solver.add(var == init_val);'''

stmt_new = '''if (vardecl->initializer) {
            z3::expr init_val = evalExpression(vardecl->initializer.get());
            solver.add(var == init_val);
            
            if (vardecl->refinement_expr) {
                pushScope();
                declareVar(vardecl->refinement_var, init_val);
                z3::expr ref_cond = ensure_bool(evalExpression(vardecl->refinement_expr.get()), ctx);
                
                solver.push();
                solver.add(!ref_cond);
                if (solver.check() == z3::sat) {
                    int l = current_node ? current_node->line : 1;
                    int c = current_node ? current_node->col : 1;
                    std::string f = current_node ? current_node->file : "";
                    throw std::runtime_error(ErrorReporter::formatError("Z3 Verification Failed: Refinement constraint not satisfied on assignment", f, l, c));
                }
                solver.pop();
                popScope();
                
                pushScope();
                declareVar(vardecl->refinement_var, var);
                z3::expr var_cond = ensure_bool(evalExpression(vardecl->refinement_expr.get()), ctx);
                popScope();
                solver.add(var_cond);
            }'''

content = content.replace(stmt_orig, stmt_new)

# Update registerContractsInDeclarations for RoutineNode
reg_orig = '''for (const auto& ens : routine->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }'''

reg_new = '''for (const auto& ens : routine->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }
                for (const auto& p : routine->params) {
                    if (p.refinement_expr) {
                        contract.requires_exprs.push_back(p.refinement_expr.get());
                    }
                }'''

content = content.replace(reg_orig, reg_new)

# Update registerContractsInDeclarations for ExternRoutineNode
reg_orig_ext = '''for (const auto& ens : ext->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }'''

reg_new_ext = '''for (const auto& ens : ext->ensures_annotations) {
                    contract.ensures_exprs.push_back(ens.get());
                }
                for (const auto& p : ext->params) {
                    if (p.refinement_expr) {
                        contract.requires_exprs.push_back(p.refinement_expr.get());
                    }
                }'''

content = content.replace(reg_orig_ext, reg_new_ext)

# Update evalAnnotationExpr to bind refinement_var
eval_orig = '''for (size_t i = 0; i < formal_params.size() && i < actual_exprs.size(); ++i) {
        declareVar(formal_params[i].name, actual_exprs[i]);
    }'''

eval_new = '''for (size_t i = 0; i < formal_params.size() && i < actual_exprs.size(); ++i) {
        declareVar(formal_params[i].name, actual_exprs[i]);
        if (!formal_params[i].refinement_var.empty()) {
            declareVar(formal_params[i].refinement_var, actual_exprs[i]);
        }
    }'''

content = content.replace(eval_orig, eval_new)

with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Updated Z3Verifier")
