import re

text = open('cpp_frontend/linker.cpp').read()
# Replace to output resolvedPath
text = text.replace('std::cerr << "[ALU CXX Linker] Resolving module: " << importNode->moduleName << std::endl;',
                    'std::cerr << "[ALU CXX Linker] Resolving module: " << importNode->moduleName << " -> " << resolvedPath << std::endl;')
open('cpp_frontend/linker.cpp', 'w').write(text)
