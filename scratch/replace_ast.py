import re

with open('cpp_frontend/ast.h', 'r') as f:
    content = f.read()

if '#include <regex>' not in content:
    content = content.replace('#include <memory>', '#include <memory>\n#include <regex>')

replace_func = '''inline std::string replaceTypeVars(std::string typeStr, const std::map<std::string, std::string>& type_map) {
    if (type_map.count(typeStr)) return type_map.at(typeStr);
    std::string result = typeStr;
    for (const auto& kv : type_map) {
        std::regex re("\\\\b" + kv.first + "\\\\b");
        result = std::regex_replace(result, re, kv.second);
    }
    return result;
}

enum class DataType'''

if 'replaceTypeVars' not in content:
    content = content.replace('enum class DataType', replace_func)

content = re.sub(r'\(type_map\.count\(([^)]+)\)\s*\?\s*type_map\.at\([^)]+\)\s*:\s*\1\)', r'replaceTypeVars(\1, type_map)', content)

with open('cpp_frontend/ast.h', 'w') as f:
    f.write(content)
