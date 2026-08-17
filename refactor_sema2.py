import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

target = '''    else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string structName;
        if (lookupStructVar(memberAcc->objectName, structName)) {'''

new_target = '''    else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string varType;
        if (lookupVar(memberAcc->objectName, varType)) {
            DataType dt = parseDataType(varType);
            if (dt == DataType::FLOAT4 || dt == DataType::FLOAT8) {
                if (memberAcc->fieldName == "x" || memberAcc->fieldName == "y" || memberAcc->fieldName == "z" || memberAcc->fieldName == "w") {
                    return {DataType::FLOAT, ""};
                }
            } else if (dt == DataType::INT4 || dt == DataType::INT8) {
                if (memberAcc->fieldName == "x" || memberAcc->fieldName == "y" || memberAcc->fieldName == "z" || memberAcc->fieldName == "w") {
                    return {DataType::INT, ""};
                }
            }
        }
        
        std::string structName;
        if (lookupStructVar(memberAcc->objectName, structName)) {'''

content = content.replace(target, new_target)

target2 = '''    else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        std::string structName;
        if (!lookupStructVar(memberAssign->objectName, structName)) {'''

new_target2 = '''    else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        std::string varType;
        bool is_vector = false;
        DataType vec_base = DataType::UNKNOWN;
        
        if (lookupVar(memberAssign->objectName, varType)) {
            DataType dt = parseDataType(varType);
            if (dt == DataType::FLOAT4 || dt == DataType::FLOAT8) { is_vector = true; vec_base = DataType::FLOAT; }
            if (dt == DataType::INT4 || dt == DataType::INT8) { is_vector = true; vec_base = DataType::INT; }
        }
        
        if (is_vector) {
            if (memberAssign->fieldName != "x" && memberAssign->fieldName != "y" && memberAssign->fieldName != "z" && memberAssign->fieldName != "w") {
                throw std::runtime_error("Semantic Error: Invalid SIMD component '" + memberAssign->fieldName + "'.");
            }
            TypeInfo rhs = checkExpression(memberAssign->expr.get());
            if (rhs.type != vec_base) {
                throw std::runtime_error("Semantic Error: SIMD assignment type mismatch.");
            }
            return;
        }

        std::string structName;
        if (!lookupStructVar(memberAssign->objectName, structName)) {'''

content = content.replace(target2, new_target2)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("Sema swizzling updated")
