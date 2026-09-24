#include "lexer.h"
#include "parser.h"
#include "llvm_codegen.h"
#include <iostream>
#include <sstream>

int main() {
    std::ostringstream source;
    source << std::cin.rdbuf();
    try {
        Lexer lexer(source.str());
        Parser parser(lexer.tokenize(), "regression.alu");
        auto program = parser.parse();
        LLVMCodeGen codegen;
        program->codegen(codegen);
        std::cout << codegen.getIR();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
