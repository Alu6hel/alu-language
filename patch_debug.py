import sys

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

replacement = """
      std::cerr << "[DEBUG] INSTANTIATED TEMPLATE: " << typeStr << " FIELDS: ";
      for(const auto& f : new_fields) std::cerr << f.name << " : " << f.type << " ; ";
      std::cerr << std::endl;
      current_ast->declarations.push_back(std::move(new_struct));
"""
content = content.replace("current_ast->declarations.push_back(std::move(new_struct));", replacement)

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)
