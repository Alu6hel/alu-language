cmake_minimum_required(VERSION 3.10)
project(alu_cxx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(alu_cxx main.cpp lexer.cpp parser.cpp semantic_analyzer.cpp llvm_codegen.cpp linker.cpp error_reporter.cpp)

# ALU Package Manager
add_executable(alupm
    alupm.cpp
    toml_parser.cpp
    semver.cpp
    dependency_resolver.cpp
    package_fetcher.cpp
    build_driver.cpp
    registry.cpp
)

# ALU Foreign Function Interface Generator (bindgen)
add_executable(alu_bindgen
    alu_bindgen_main.cpp
    bindgen.cpp
)

