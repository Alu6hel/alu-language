import re

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

# MemberAccessNode
target = '''std::string LLVMCodeGen::visit(MemberAccessNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string structName = lookupStructType(node->objectName);'''

new_target = '''std::string LLVMCodeGen::visit(MemberAccessNode* node) {
    if (emit_debug_info) current_debug_node = node;
    
    std::string aluType = lookupAluType(node->objectName);
    if (aluType == "float4" || aluType == "float8" || aluType == "int4" || aluType == "int8") {
        int idx = 0;
        if (node->fieldName == "x") idx = 0;
        else if (node->fieldName == "y") idx = 1;
        else if (node->fieldName == "z") idx = 2;
        else if (node->fieldName == "w") idx = 3;
        
        std::string obj_ir = lookupIRName(node->objectName);
        std::string llvmType = getLLVMType(aluType);
        
        std::string vec_val = getTempReg();
        emit("  " + vec_val + " = load " + llvmType + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        
        std::string res = getTempReg();
        emit("  " + res + " = extractelement " + llvmType + " " + vec_val + ", i32 " + std::to_string(idx), node);
        return res;
    }

    std::string structName = lookupStructType(node->objectName);'''

content = content.replace(target, new_target)

# MemberAssignNode
target2 = '''void LLVMCodeGen::visit(MemberAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string structName = lookupStructType(node->objectName);'''

new_target2 = '''void LLVMCodeGen::visit(MemberAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    
    std::string aluType = lookupAluType(node->objectName);
    if (aluType == "float4" || aluType == "float8" || aluType == "int4" || aluType == "int8") {
        int idx = 0;
        if (node->fieldName == "x") idx = 0;
        else if (node->fieldName == "y") idx = 1;
        else if (node->fieldName == "z") idx = 2;
        else if (node->fieldName == "w") idx = 3;
        
        std::string obj_ir = lookupIRName(node->objectName);
        std::string llvmType = getLLVMType(aluType);
        
        std::string vec_val = getTempReg();
        emit("  " + vec_val + " = load " + llvmType + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        
        std::string rhs_reg = evaluateExpression(node->expr.get());
        std::string res = getTempReg();
        std::string base_type = "float";
        if (aluType == "int4" || aluType == "int8") base_type = "i32";
        
        emit("  " + res + " = insertelement " + llvmType + " " + vec_val + ", " + base_type + " " + rhs_reg + ", i32 " + std::to_string(idx), node);
        emit("  store " + llvmType + " " + res + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        return;
    }

    std::string structName = lookupStructType(node->objectName);'''

content = content.replace(target2, new_target2)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVMCodeGen struct access updated for SIMD")
