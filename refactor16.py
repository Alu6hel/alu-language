import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

# evalExpression MemberAccessNode
eval_orig = '''} else if (auto varNode = dynamic_cast<VarAccessNode*>(expr)) {
        return getVar(varNode->name);'''

eval_new = '''} else if (auto varNode = dynamic_cast<VarAccessNode*>(expr)) {
        return getVar(varNode->name);
    } else if (auto memberAccess = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string full_name = memberAccess->objectName + "_" + memberAccess->fieldName;
        return getVar(full_name);'''

content = content.replace(eval_orig, eval_new)

# checkStatement MemberAssignNode
stmt_orig = '''} else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        z3::expr new_val = evalExpression(varassign->expr.get());'''

stmt_new = '''} else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        z3::expr new_val = evalExpression(varassign->expr.get());'''

# Add member assign before var assign
stmt_orig = '''} else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {'''
stmt_new = '''} else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        z3::expr new_val = evalExpression(memberAssign->expr.get());
        std::string full_name = memberAssign->objectName + "_" + memberAssign->fieldName;
        z3::expr var = ctx.int_const((full_name + "_new").c_str());
        declareVar(full_name, var);
        solver.add(var == new_val);
    } else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {'''

content = content.replace(stmt_orig, stmt_new)


with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Updated Z3Verifier for MemberAccess and MemberAssign")
