import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

replacement = '''      if (type.find("ptr<") == 0 && type.back() == '>') {
          std::string base = type.substr(4, type.length() - 5);
          while (!base.empty() && base.back() == ' ') base.pop_back();
          return getLLVMType(base) + "*";
      }
      if (type.find("*") != std::string::npos) {'''

content = content.replace('      if (type.find("*") != std::string::npos) {', replacement)
with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
