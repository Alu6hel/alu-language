@echo off
clang++ -std=c++17 cpp_frontend\lsp_server.cpp cpp_frontend\lexer.cpp cpp_frontend\parser.cpp cpp_frontend\semantic_analyzer.cpp cpp_frontend\error_reporter.cpp cpp_frontend\z3_verifier.cpp cpp_frontend\llvm_codegen.cpp -I z3\include -L z3\bin -lz3 -o alu-lsp.exe
echo EXIT CODE: %ERRORLEVEL%
