import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

req_orig = '''// To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        std::cout << "[DEBUG] Precondition: " << precondition << std::endl;
        solver.add(!precondition);
        
        if (solver.check() == z3::sat) {
            z3::model m = solver.get_model();
//             std::cerr << "\\n[ALU CXX Z3 FATAL] @requires Contract Violation Detected!" << std::endl;
//             std::cerr << "  Function: '" << calleeName << "'" << std::endl;
//             std::cerr << "  Precondition may not hold at this call site." << std::endl;
//             std::cerr << "  Z3 Counterexample: " << m << std::endl;
            int l = current_node ? current_node->line : 1;
            int c = current_node ? current_node->col : 1;'''

req_new = '''// To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!precondition);
        
        if (solver.check() == z3::sat) {
            z3::model m = solver.get_model();
            std::cerr << "\\n[ALU CXX Z3 FATAL] @requires Contract Violation Detected!" << std::endl;
            std::cerr << "  Function: '" << calleeName << "'" << std::endl;
            std::cerr << "  Precondition may not hold at this call site." << std::endl;
            std::cerr << "  Z3 Counterexample: " << m << std::endl;
            int l = current_node ? current_node->line : 1;
            int c = current_node ? current_node->col : 1;'''

content = content.replace(req_orig, req_new)

with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Restored formatting")
