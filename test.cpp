#include <iostream>
#include <string>
#include <regex>

int main() {
    std::string result = "ptr<HashEntry<K, V> >";
    std::regex re("\\bK\\b");
    result = std::regex_replace(result, re, "string");
    std::cout << result << std::endl;
}
