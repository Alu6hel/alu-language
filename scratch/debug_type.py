import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

replacement = '''std::string LLVMCodeGen::getLLVMType(const std::string& type) {
      std::cerr << "GETLLVMTYPE: '" << type << "'" << std::endl;
      if (type.find("ptr<") == 0 && type.back() == '>') {'''

content = content.replace('std::string LLVMCodeGen::getLLVMType(const std::string& type) {\n      if (type.find("ptr<") == 0 && type.back() == \'>\') {', replacement)
with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
