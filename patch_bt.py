import sys
import re

text = open('cpp_frontend/semantic_analyzer.cpp').read()

text = re.sub(
    r'void SemanticAnalyzer::instantiateTemplateIfNeeded\(const std::string& typeStr\) \{',
    r'''#include <execinfo.h>
#include <unistd.h>
void SemanticAnalyzer::instantiateTemplateIfNeeded(const std::string& typeStr) {
    if (typeStr == "Vector<T>") {
        void* array[10];
        size_t size = backtrace(array, 10);
        std::cerr << "CALL STACK FOR Vector<T>:\\n";
        backtrace_symbols_fd(array, size, STDERR_FILENO);
    }
''',
    text, count=1
)

open('cpp_frontend/semantic_analyzer.cpp', 'w').write(text)
