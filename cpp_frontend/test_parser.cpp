
#include "parser.h"
#include "lexer.h"
#include <iostream>

int main() {
    std::string source = "routine main() -> int { int n = vec_len<string>(parts); return 0; }";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, "test.alu");
    try {
        auto ast = parser.parse();
        ast->print();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}

