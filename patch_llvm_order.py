import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# We want to replace our previous opaque injection
# In getIR() we had:
# return opaque_str + "\\n" + ir_output.str() + "\\n" + global_strings_output.str() + di_str;

# Let's just find that string and replace it with something that inserts opaque_str in the correct place.

old_return = 'return opaque_str + "\\n" + ir_output.str() + "\\n" + global_strings_output.str() + di_str;'
new_return = '''
    std::string ir_str = ir_output.str();
    size_t pos = ir_str.find("target triple = \\"x86_64-pc-windows-msvc\\"\\n\\n");
    if (pos != std::string::npos) {
        ir_str.insert(pos + 46, opaque_str + "\\n");
    } else {
        ir_str = opaque_str + "\\n" + ir_str;
    }
    return ir_str + "\\n" + global_strings_output.str() + di_str;
'''

if old_return in c:
    c = c.replace(old_return, new_return)
    print("PATCH APPLIED SUCCESSFULLY")
else:
    print("COULD NOT FIND PREVIOUS PATCH IN llvm_codegen.cpp")

open(f, 'w').write(c)
