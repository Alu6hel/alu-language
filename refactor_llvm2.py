import re

with open('cpp_frontend/llvm_codegen.h', 'r') as f:
    content = f.read()

target = '''    void visit(VarDeclNode* node);'''

new_target = '''    std::string visit(VectorInitNode* node);
    void visit(VarDeclNode* node);'''

content = content.replace(target, new_target)

with open('cpp_frontend/llvm_codegen.h', 'w') as f:
    f.write(content)

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

target2 = '''std::string LLVMCodeGen::visit(LiteralNode* node) {'''

new_target2 = '''std::string LLVMCodeGen::visit(VectorInitNode* node) {
    std::string llvm_type = getLLVMType(node->typeName);
    std::string current_vec = "undef";
    
    std::string base_type = "float";
    if (node->typeName == "int4" || node->typeName == "int8") base_type = "i32";
    
    for (size_t i = 0; i < node->elements.size(); ++i) {
        std::string val_reg = evaluateExpression(node->elements[i].get());
        std::string next_vec = getTempReg();
        emit("  " + next_vec + " = insertelement " + llvm_type + " " + current_vec + ", " + base_type + " " + val_reg + ", i32 " + std::to_string(i), node);
        current_vec = next_vec;
    }
    return current_vec;
}

std::string LLVMCodeGen::visit(LiteralNode* node) {'''

content = content.replace(target2, new_target2)

target3 = '''    } else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        return visit(memberAcc);'''

new_target3 = '''    } else if (auto vecInit = dynamic_cast<VectorInitNode*>(expr)) {
        return visit(vecInit);
    } else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        return visit(memberAcc);'''

content = content.replace(target3, new_target3)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVMCodeGen VectorInitNode updated")
