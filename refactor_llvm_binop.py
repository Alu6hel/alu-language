import re

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

target = '''    bool isFloat = (ltype == "float" || rtype == "float" || ltype == "double" || rtype == "double");
    std::string opType = "i32";
    if (ltype == "double" || rtype == "double") opType = "double";
    else if (ltype == "float" || rtype == "float") opType = "float";
    else if (ltype == "i8" && rtype == "i8") opType = "i8";
    else if (ltype == "i1" && rtype == "i1") opType = "i1";'''

new_target = '''    bool isFloat = (ltype == "float" || rtype == "float" || ltype == "double" || rtype == "double" || ltype == "<4 x float>" || ltype == "<8 x float>");
    std::string opType = "i32";
    if (ltype == "double" || rtype == "double") opType = "double";
    else if (ltype == "float" || rtype == "float") opType = "float";
    else if (ltype == "<4 x float>") opType = "<4 x float>";
    else if (ltype == "<8 x float>") opType = "<8 x float>";
    else if (ltype == "<4 x i32>") opType = "<4 x i32>";
    else if (ltype == "<8 x i32>") opType = "<8 x i32>";
    else if (ltype == "i8" && rtype == "i8") opType = "i8";
    else if (ltype == "i1" && rtype == "i1") opType = "i1";'''

content = content.replace(target, new_target)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVMCodeGen BinOp updated for SIMD")
