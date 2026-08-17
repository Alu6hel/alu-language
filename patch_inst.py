import sys

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

old_loop = """
    std::vector<StructField> new_fields;
    for (const auto& f : tmpl->fields) {
        std::string new_type = f.type;
        for (size_t i = 0; i < args.size(); ++i) {
            if (new_type == tmpl->type_params[i]) {
                new_type = args[i];
            }
        }
        new_fields.push_back({new_type, f.name});
    }
"""

new_loop = """
    std::map<std::string, std::string> type_map;
    for (size_t i = 0; i < args.size(); ++i) {
        type_map[tmpl->type_params[i]] = args[i];
    }
    
    std::vector<StructField> new_fields;
    for (const auto& f : tmpl->fields) {
        std::string new_type = replaceTypeVars(f.type, type_map);
        new_fields.push_back({new_type, f.name});
    }
"""

if old_loop in content:
    content = content.replace(old_loop, new_loop)
else:
    print("Could not find old loop to replace.")

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)
