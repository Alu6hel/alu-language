import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# Update parseDataType
target = '''    if (base == "int") return DataType::INT;
    if (base == "float") return DataType::FLOAT;
    if (base == "double") return DataType::DOUBLE;
    if (base == "string") return DataType::STRING;
    if (base == "bool") return DataType::BOOL;
    if (base == "void") return DataType::VOID;
    if (base == "byte") return DataType::BYTE;'''

new_target = '''    if (base == "int") return DataType::INT;
    if (base == "float") return DataType::FLOAT;
    if (base == "double") return DataType::DOUBLE;
    if (base == "string") return DataType::STRING;
    if (base == "bool") return DataType::BOOL;
    if (base == "void") return DataType::VOID;
    if (base == "byte") return DataType::BYTE;
    if (base == "float4") return DataType::FLOAT4;
    if (base == "float8") return DataType::FLOAT8;
    if (base == "int4") return DataType::INT4;
    if (base == "int8") return DataType::INT8;'''

content = content.replace(target, new_target)

# Add checkExpression for VectorInitNode
target2 = '''    else if (auto funcCall = dynamic_cast<FuncCallNode*>(expr)) {'''

new_target2 = '''    else if (auto vecInit = dynamic_cast<VectorInitNode*>(expr)) {
        DataType targetDT = parseDataType(vecInit->typeName);
        int expected_args = 0;
        DataType expected_base = DataType::UNKNOWN;
        
        if (targetDT == DataType::FLOAT4) { expected_args = 4; expected_base = DataType::FLOAT; }
        else if (targetDT == DataType::FLOAT8) { expected_args = 8; expected_base = DataType::FLOAT; }
        else if (targetDT == DataType::INT4) { expected_args = 4; expected_base = DataType::INT; }
        else if (targetDT == DataType::INT8) { expected_args = 8; expected_base = DataType::INT; }
        
        if (vecInit->elements.size() != expected_args) {
            throw std::runtime_error("Semantic Error: SIMD Vector " + vecInit->typeName + " requires exactly " + std::to_string(expected_args) + " arguments.");
        }
        
        for (const auto& el : vecInit->elements) {
            TypeInfo elType = checkExpression(el.get());
            if (elType.type != expected_base) {
                throw std::runtime_error("Semantic Error: SIMD Vector " + vecInit->typeName + " elements must be of base type " + DataTypeToString(expected_base) + ".");
            }
        }
        return {targetDT, ""};
    }
    else if (auto funcCall = dynamic_cast<FuncCallNode*>(expr)) {'''

content = content.replace(target2, new_target2)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("Sema parseDataType and checkExpression for VectorInit updated")
