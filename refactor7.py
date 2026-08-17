with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

# Fix getLLVMType in llvm_codegen.cpp
old_getLLVMType = '''std::string LLVMCodeGen::getLLVMType(const std::string& type) {
        static std::ofstream d_log("dump_types.log", std::ios_base::app);
        d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
        
        if (type.find("ptr<") == 0 || type.find("managed<") == 0) {'''

new_getLLVMType = '''std::string LLVMCodeGen::getLLVMType(const std::string& type) {
        static std::ofstream d_log("dump_types.log", std::ios_base::app);
        d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
        
        std::string clean_type = type;
        size_t unit_start = clean_type.find('<');
        if (unit_start != std::string::npos && clean_type.find("ptr<") != 0 && clean_type.find("managed<") != 0 && clean_type.find("array<") != 0 && clean_type.find("routine<") != 0) {
            clean_type = clean_type.substr(0, unit_start);
        }
        
        if (type.find("ptr<") == 0 || type.find("managed<") == 0) {'''
        
content = content.replace(old_getLLVMType, new_getLLVMType)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVM CodeGen Fixed")

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

content = content.replace('sym.hover_text = "variable " + varNode->name + " : " + DataTypeToString(t);', 'sym.hover_text = "variable " + varNode->name + " : " + DataTypeToString(meta.type) + (meta.unit.empty() ? "" : "<" + meta.unit + ">");')
content = content.replace('        return {t, ""};', '        return {meta.type, meta.unit};')

with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print("Semantic Analyzer Fixed")

