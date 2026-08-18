@echo off
clang++ -std=c++17 ^
cpp_frontend\alupm.cpp ^
cpp_frontend\build_driver.cpp ^
cpp_frontend\dependency_resolver.cpp ^
cpp_frontend\error_reporter.cpp ^
cpp_frontend\lexer.cpp ^
cpp_frontend\linker.cpp ^
cpp_frontend\llvm_codegen.cpp ^
cpp_frontend\package_fetcher.cpp ^
cpp_frontend\parser.cpp ^
cpp_frontend\registry.cpp ^
cpp_frontend\semantic_analyzer.cpp ^
cpp_frontend\semver.cpp ^
cpp_frontend\toml_parser.cpp ^
cpp_frontend\z3_verifier.cpp ^
-I z3\include -L z3\bin -lz3 -o alupm.exe
echo EXIT CODE: %ERRORLEVEL%
