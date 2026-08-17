import sys

with open('cpp_frontend/llvm_codegen.cpp', 'r') as f:
    content = f.read()

content = content.replace('emit("  " + reg + " = call i8* @alu_alloc(i64 " + size_int + ")");', 'emit("  " + reg + " = call i8* @__alu_alloc_internal(i32 " + size_int + ")");')
content = content.replace('emit("declare i8* @alu_alloc(i64)");', 'emit("declare i8* @__alu_alloc_internal(i32)");')

with open('cpp_frontend/llvm_codegen.cpp', 'w') as f:
    f.write(content)
