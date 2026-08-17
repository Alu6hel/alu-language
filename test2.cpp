#include <iostream>
#include <string>
#include <regex>
#include <map>

int main() {
    std::string result = "ptr<T>";
    std::map<std::string, std::string> type_map = {{"T", "string"}};
    for (const auto& kv : type_map) {
        std::regex re("\\b" + kv.first + "\\b");
        result = std::regex_replace(result, re, kv.second);
    }
    std::cout << result << std::endl;
}
