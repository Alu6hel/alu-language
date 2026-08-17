#include <iostream>
#include <string>

std::string getLLVMType(const std::string& type) {
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
    if (type == "int") return "i32";
    if (type == "routine") return "i8*";
    return "%" + type + "*";
}

int main() {
    std::cout << getLLVMType("ptr<routine>") << std::endl;
    return 0;
}
