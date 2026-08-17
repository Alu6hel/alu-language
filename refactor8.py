with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

bad_lookup = '''
            DataType t;
            if (!lookupSymbol(literal->value, t)) {
                throw std::runtime_error("Semantic Error: Undefined variable '" + literal->value + "'");
            }
            return {meta.type, meta.unit};
'''
good_lookup = '''
            SymbolMeta meta;
            if (!lookupSymbolMeta(literal->value, meta)) {
                throw std::runtime_error("Semantic Error: Undefined variable '" + literal->value + "'");
            }
            return {meta.type, meta.unit};
'''
content = content.replace(bad_lookup.strip(), good_lookup.strip())

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

bad_llvm = '''
    if (type.find("ptr<") == 0 || type.find("managed<") == 0) {
        size_t pos1 = type.find("<");
        size_t pos2 = type.rfind(">");
        if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
            std::string base = type.substr(pos1 + 1, pos2 - pos1 - 1);
            while (!base.empty() && base.back() == ' ') base.pop_back();
            while (!base.empty() && base.front() == ' ') base.erase(0, 1);
            return getLLVMType(base) + "*";
        }
    }
'''

good_llvm = '''
    std::string clean_type = type;
    size_t unit_start = clean_type.find('<');
    if (unit_start != std::string::npos && clean_type.find("ptr<") != 0 && clean_type.find("managed<") != 0 && clean_type.find("array<") != 0 && clean_type.find("routine<") != 0) {
        clean_type = clean_type.substr(0, unit_start);
    }

    if (type.find("ptr<") == 0 || type.find("managed<") == 0) {
        size_t pos1 = type.find("<");
        size_t pos2 = type.rfind(">");
        if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
            std::string base = type.substr(pos1 + 1, pos2 - pos1 - 1);
            while (!base.empty() && base.back() == ' ') base.pop_back();
            while (!base.empty() && base.front() == ' ') base.erase(0, 1);
            return getLLVMType(base) + "*";
        }
    }
'''

content = content.replace(bad_llvm.strip(), good_llvm.strip())

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("Fixes applied.")
