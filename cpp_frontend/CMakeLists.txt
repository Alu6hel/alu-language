cmake_minimum_required(VERSION 3.10)
project(alu_cxx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(alu_cxx main.cpp lexer.cpp parser.cpp)
