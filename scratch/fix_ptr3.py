import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

replacement = '''std::string LLVMCodeGen::getLLVMType(const std::string& type) {
      static std::ofstream d_log("dump_types.log", std::ios_base::app);
      d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
      
      if (type.find("ptr") == 0) {
          size_t pos1 = type.find("<");
          size_t pos2 = type.rfind(">");
          if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
              std::string base = type.substr(pos1 + 1, pos2 - pos1 - 1);
              while (!base.empty() && base.back() == ' ') base.pop_back();
              while (!base.empty() && base.front() == ' ') base.erase(0, 1);
              return getLLVMType(base) + "*";
          }
      }
'''

idx = content.find('std::string LLVMCodeGen::getLLVMType(const std::string& type) {')
idx_end = content.find('if (type == "int")', idx)
content = content[:idx] + replacement + "      " + content[idx_end:]

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write('#include <fstream>\n' + content)
