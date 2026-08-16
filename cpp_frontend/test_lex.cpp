
#include <iostream>
#include <vector>
#include "lexer.h"

int main() {
    Lexer lexer("routine main() -> int { int n = vec_len<string>(parts); return 0; }");
    auto tokens = lexer.tokenize();
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << i << ": " << tokens[i].value << " (type " << (int)tokens[i].type << ")\n";
    }
    return 0;
}

