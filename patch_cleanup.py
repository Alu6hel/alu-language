import sys

text = open('cpp_frontend/semantic_analyzer.cpp').read()
text = text.replace('std::cerr << "[DEBUG] parseDataType: " << rawTypeStr << std::endl;\n', '')
text = text.replace('std::cerr << "[DEBUG] checkExpression line " << expr->line << " node: " << typeid(*expr).name() << std::endl;\n', '')
open('cpp_frontend/semantic_analyzer.cpp', 'w').write(text)
