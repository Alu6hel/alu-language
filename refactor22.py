import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

# Fix the broken replacement
content = content.replace(
'''        if (var_to_id.count(varAccess->name)) {
//                 std::cerr << "  Variable: '" << varAccess->name << "' was moved or freed." << std::endl;
//                 std::cerr << "  Z3 Counterexample: " << m << std::endl;
                int l = current_node ? current_node->line : 1;''',
'''        if (var_to_id.count(varAccess->name)) {
            int v_id = var_to_id[varAccess->name];
            z3::expr is_alive = z3::select(var_alive, ctx.int_val(v_id));
            solver.push();
            solver.add(!is_alive);
            if (solver.check() == z3::sat) {
                z3::model m = solver.get_model();
//                 std::cerr << "\\n[ALU CXX Z3 FATAL] Use-After-Move Violation Detected!" << std::endl;
//                 std::cerr << "  Variable: '" << varAccess->name << "' was moved or freed." << std::endl;
//                 std::cerr << "  Z3 Counterexample: " << m << std::endl;
                int l = current_node ? current_node->line : 1;'''
)

# Find the location to insert MemberAccessNode logic
target = '''        return getVar(varAccess->name);
    } else if (auto binop = dynamic_cast<BinOpNode*>(expr)) {'''

new_target = '''        return getVar(varAccess->name);
    } else if (auto memberAccess = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string full_name = memberAccess->objectName + "_" + memberAccess->fieldName;
        return getVar(full_name);
    } else if (auto binop = dynamic_cast<BinOpNode*>(expr)) {'''

content = content.replace(target, new_target)

with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Fixed evalExpression properly")
