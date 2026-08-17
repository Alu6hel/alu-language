import sys

content = open('cpp_frontend/semantic_analyzer.cpp').read()
content = content.replace('DataType SemanticAnalyzer::parseDataType(const std::string& rawTypeStr) {', 'DataType SemanticAnalyzer::parseDataType(const std::string& rawTypeStr) {\n    std::cerr << "[DEBUG] parseDataType: " << rawTypeStr << std::endl;')
open('cpp_frontend/semantic_analyzer.cpp', 'w').write(content)
