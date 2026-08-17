#include <iostream>
#include <regex>
#include <string>
#include <map>

inline std::string replaceTypeVars(std::string typeStr, const std::map<std::string, std::string>& type_map) {
    if (type_map.count(typeStr)) return type_map.at(typeStr);
    std::string result = typeStr;
    for (const auto& kv : type_map) {
        std::regex re("\\b" + kv.first + "\\b");
        result = std::regex_replace(result, re, kv.second);
    }
    return result;
}

int main() {
    std::map<std::string, std::string> map = {{"K", "string"}, {"V", "int"}};
    std::cout << replaceTypeVars("ptr<ptr<HashEntry<K, V>>>", map) << std::endl;
}
