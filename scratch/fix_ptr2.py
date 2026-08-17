import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

replacement = '''std::string LLVMCodeGen::getLLVMType(const std::string& type) {
      if (type.find("ptr<") == 0 && type.back() == '>') {
          std::string base = type.substr(4, type.length() - 5);
          while (!base.empty() && base.back() == ' ') base.pop_back();
          return getLLVMType(base) + "*";
      }
      
      // Also handle potential spaces like "ptr <" or "ptr< HashMap"
      if (type.find("ptr") == 0) {
          size_t pos = type.find("<");
          if (pos != std::string::npos && type.back() == '>') {
              std::string base = type.substr(pos + 1, type.length() - pos - 2);
              while (!base.empty() && base.back() == ' ') base.pop_back();
              while (!base.empty() && base.front() == ' ') base.erase(0, 1);
              return getLLVMType(base) + "*";
          }
      }
'''

content = content.replace('std::string LLVMCodeGen::getLLVMType(const std::string& type) {', replacement)
with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
