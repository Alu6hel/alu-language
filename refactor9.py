import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

start_idx = content.find('DataType rightT = rightT_info.type;')
end_idx = content.find('if (binOp->op == "==" || binOp->op == "!=" || binOp->op == "<" || binOp->op == "<=" || binOp->op == ">" || binOp->op == ">=") {')

binop_logic = '''
        std::cout << "[DEBUG] BinOp " << binOp->op << " between <" << leftT_info.unit << "> and <" << rightT_info.unit << ">" << std::endl;

        std::string res_unit = "";
        if (!leftT_info.unit.empty() || !rightT_info.unit.empty()) {
            if (binOp->op == "+" || binOp->op == "-") {
                if (leftT_info.unit != rightT_info.unit && (!leftT_info.unit.empty() && !rightT_info.unit.empty())) {
                    throw std::runtime_error("Semantic Error: Unit mismatch in addition/subtraction. <" + leftT_info.unit + "> vs <" + rightT_info.unit + ">");
                }
                res_unit = leftT_info.unit.empty() ? rightT_info.unit : leftT_info.unit;
            } else if (binOp->op == "*") {
                auto lu = parseUnit(leftT_info.unit);
                auto ru = parseUnit(rightT_info.unit);
                for (auto& kv : ru) lu[kv.first] += kv.second;
                res_unit = formatUnit(lu);
            } else if (binOp->op == "/") {
                auto lu = parseUnit(leftT_info.unit);
                auto ru = parseUnit(rightT_info.unit);
                for (auto& kv : ru) lu[kv.first] -= kv.second;
                res_unit = formatUnit(lu);
            } else {
                res_unit = leftT_info.unit.empty() ? rightT_info.unit : leftT_info.unit;
            }
        }
'''

content = content[:start_idx + len('DataType rightT = rightT_info.type;')] + '\n' + binop_logic + '\n        ' + content[end_idx:]

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("BinOp logic restored and debug log added.")
