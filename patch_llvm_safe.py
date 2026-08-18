import sys
import re

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# Fix the base return
c = c.replace(
    'return "%" + getNamespacedName(base) + "*";',
    '{ std::string n = getNamespacedName(base); opaque_types.insert(n); return "%" + n + "*"; }'
)

# Fix the clean_type return
c = c.replace(
    'return "%" + getNamespacedName(clean_type) + "*";',
    '{ std::string n = getNamespacedName(clean_type); opaque_types.insert(n); return "%" + n + "*"; }'
)

# Fix getIR()
c = re.sub(
    r'return ir_output\.str\(\) \+ \"\\n\" \+ global_strings_output\.str\(\) \+ di_str;',
    'std::string opaque_str = "";\n    for(const auto& ot : opaque_types) { opaque_str += "%" + ot + " = type opaque\\n"; }\n    return opaque_str + "\\n" + ir_output.str() + "\\n" + global_strings_output.str() + di_str;',
    c
)

open(f, 'w').write(c)
