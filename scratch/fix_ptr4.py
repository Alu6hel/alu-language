import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

replacement = '''void LLVMCodeGen::declareStructType(const std::string& name, const std::string& structName) {
      if (!struct_type_stack.empty()) {
          std::string actualName = structName;
          if (actualName.find("ptr") == 0) {
              size_t pos1 = actualName.find("<");
              size_t pos2 = actualName.rfind(">");
              if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
                  actualName = actualName.substr(pos1 + 1, pos2 - pos1 - 1);
                  while (!actualName.empty() && actualName.back() == ' ') actualName.pop_back();
                  while (!actualName.empty() && actualName.front() == ' ') actualName.erase(0, 1);
              }
          }
          struct_type_stack.back()[name] = getNamespacedName(actualName);
      }
  }'''

old_str = '''void LLVMCodeGen::declareStructType(const std::string& name, const std::string& structName) {
      if (!struct_type_stack.empty()) {
          struct_type_stack.back()[name] = getNamespacedName(structName);
      }
  }'''

content = content.replace(old_str, replacement)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
