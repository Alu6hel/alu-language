import re

with open('cpp_frontend/z3_verifier.cpp', 'r') as f:
    content = f.read()

req_orig = '''// To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        solver.add(!precondition);'''

req_new = '''// To verify the precondition holds, check if its negation is satisfiable
        solver.push();
        std::cout << \"[DEBUG] Precondition: \" << precondition << std::endl;
        solver.add(!precondition);'''

content = content.replace(req_orig, req_new)
with open('cpp_frontend/z3_verifier.cpp', 'w') as f:
    f.write(content)

print("Updated with debug print")
