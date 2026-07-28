#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "llvm_codegen.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: alu_cxx <file.alu>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    std::cout << "[ALU CXX] Compiling: " << filename << std::endl;
    std::cout << "[ALU CXX] Lexical Analysis (Scanning)..." << std::endl;
    
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    
    std::cout << "[ALU CXX] Syntactic Analysis (Parsing)..." << std::endl;
    
    Parser parser(tokens);
    try {
        std::unique_ptr<ProgramNode> ast = parser.parse();
        std::cout << "[ALU CXX] Abstract Syntax Tree generated successfully:" << std::endl;
        std::cout << "==================================================" << std::endl;
        ast->print();
        std::cout << "==================================================" << std::endl;
        
        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.analyze(ast.get());
        
        std::cout << "[ALU CXX] Ready for LLVM IR Translation." << std::endl;
        
        LLVMCodeGen codegen;
        ast->codegen(codegen);
        
        std::string outFilename = filename + ".ll";
        codegen.saveToFile(outFilename);
        
        std::cout << "==================================================" << std::endl;
        std::cout << codegen.getIR();
        std::cout << "==================================================" << std::endl;
        std::cout << "[ALU CXX] Successfully wrote IR to " << outFilename << std::endl;
        
        // --- BACKEND LINKER PHASE --- //
        std::cout << "[ALU CXX] Invoking LLVM Backend (clang) to assemble and link..." << std::endl;
        
        // Strip the .alu extension and add .exe
        std::string baseFilename = filename.substr(0, filename.find_last_of("."));
        std::string exeFilename = baseFilename + ".exe";
        
        std::string compileCommand = "clang -O3 -o " + exeFilename + " " + outFilename;
        int result = std::system(compileCommand.c_str());
        
        if (result == 0) {
            std::cout << "[ALU CXX] Compilation Successful! Executable built at: " << exeFilename << std::endl;
        } else {
            std::cerr << "[ALU CXX] Linker Error: Clang failed with exit code " << result << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n[COMPILER ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
