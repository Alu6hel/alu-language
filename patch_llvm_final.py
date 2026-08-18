import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# Replace getNamespacedName in array type
c = c.replace(
    'return "%" + getNamespacedName(base) + "*";',
    '{ std::string n = getNamespacedName(base); opaque_types.insert(n); return "%" + n + "*"; }'
)

# Replace the getIR return line EXACTLY
target_line = 'return ir_output.str() + "\\n" + global_strings_output.str() + di_str;'
if target_line in c:
    replacement = '''
    std::string opaque_str = "";
    for(const auto& ot : opaque_types) { 
        opaque_str += "%" + ot + " = type opaque\\n"; 
    }
    return opaque_str + "\\n" + ir_output.str() + "\\n" + global_strings_output.str() + di_str;
'''
    c = c.replace(target_line, replacement)
    print("PATCH APPLIED SUCCESSFULLY")
else:
    print("TARGET LINE NOT FOUND IN getIR")

open(f, 'w').write(c)
