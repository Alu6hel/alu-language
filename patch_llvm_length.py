import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# Replace the broken insert with a proper replacement
broken = '''    std::string ir_str = ir_output.str();
    size_t pos = ir_str.find("target triple = \\"x86_64-pc-windows-msvc\\"\\n\\n");
    if (pos != std::string::npos) {
        ir_str.insert(pos + 46, opaque_str + "\\n");
    } else {
        ir_str = opaque_str + "\\n" + ir_str;
    }
    return ir_str + "\\n" + global_strings_output.str() + di_str;'''

fixed = '''    std::string ir_str = ir_output.str();
    std::string target = "target triple = \\"x86_64-pc-windows-msvc\\"\\n\\n";
    size_t pos = ir_str.find(target);
    if (pos != std::string::npos) {
        ir_str.insert(pos + target.length(), opaque_str + "\\n");
    } else {
        ir_str = opaque_str + "\\n" + ir_str;
    }
    return ir_str + "\\n" + global_strings_output.str() + di_str;'''

if broken in c:
    c = c.replace(broken, fixed)
    print("FIX APPLIED")
else:
    print("BROKEN NOT FOUND")

open(f, 'w').write(c)
