import re

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

target = '''void NamespaceNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }'''

new_target = '''void NamespaceNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void VectorInitNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }'''

content = content.replace(target, new_target)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVMCodeGen VectorInitNode::codegen updated")
