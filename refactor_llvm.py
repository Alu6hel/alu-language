import re

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

target = '''    if (clean_type == "byte") return "i8";'''

new_target = '''    if (clean_type == "byte") return "i8";
    if (clean_type == "float4") return "<4 x float>";
    if (clean_type == "float8") return "<8 x float>";
    if (clean_type == "int4") return "<4 x i32>";
    if (clean_type == "int8") return "<8 x i32>";'''

content = content.replace(target, new_target)

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)

print("LLVMCodeGen getLLVMType updated")
