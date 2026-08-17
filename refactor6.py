with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

old_getLLVMType = '''
std::string LLVMCodeGen::getLLVMType(const std::string& type) {
        static std::ofstream d_log("dump_types.log", std::ios_base::app);
        d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
'''
new_getLLVMType = '''
std::string LLVMCodeGen::getLLVMType(const std::string& type) {
        static std::ofstream d_log("dump_types.log", std::ios_base::app);
        d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
        
        std::string clean_type = type;
        size_t unit_start = clean_type.find('<');
        if (unit_start != std::string::npos && clean_type.find("ptr<") != 0 && clean_type.find("managed<") != 0 && clean_type.find("array<") != 0 && clean_type.find("routine<") != 0) {
            clean_type = clean_type.substr(0, unit_start);
        }
'''
content = content.replace(old_getLLVMType.strip(), new_getLLVMType.strip())

# And change usages of 	ype inside the rest of getLLVMType to clean_type
# I'll just do it manually for the primitive type checks inside getLLVMType
content = content.replace('if (type == "int") return "i32";', 'if (clean_type == "int") return "i32";')
content = content.replace('if (type == "float") return "float";', 'if (clean_type == "float") return "float";')
content = content.replace('if (type == "double") return "double";', 'if (clean_type == "double") return "double";')
content = content.replace('if (type == "bool") return "i1";', 'if (clean_type == "bool") return "i1";')
content = content.replace('if (type == "string") return "i8*";', 'if (clean_type == "string") return "i8*";')
content = content.replace('if (type == "byte") return "i8";', 'if (clean_type == "byte") return "i8";')
content = content.replace('if (type == "void") return "void";', 'if (clean_type == "void") return "void";')

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print('Refactor 6 finished.')
