import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

target = '''    else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string varType;
        if (lookupVar(memberAcc->objectName, varType)) {
            DataType dt = parseDataType(varType);'''

new_target = '''    else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        DataType dt;
        if (lookupSymbol(memberAcc->objectName, dt)) {'''

content = content.replace(target, new_target)

target2 = '''    else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        std::string varType;
        bool is_vector = false;
        DataType vec_base = DataType::UNKNOWN;
        
        if (lookupVar(memberAssign->objectName, varType)) {
            DataType dt = parseDataType(varType);'''

new_target2 = '''    else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        DataType dt;
        bool is_vector = false;
        DataType vec_base = DataType::UNKNOWN;
        
        if (lookupSymbol(memberAssign->objectName, dt)) {'''

content = content.replace(target2, new_target2)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("Semantic analyzer lookup fixed")
