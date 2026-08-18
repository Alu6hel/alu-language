import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# I need to find the `return ir_output.str() + ...` line inside getIR()
import re
c = re.sub(
    r'return ir_output\.str\(\) \+ \"\\n\" \+ global_strings_output\.str\(\) \+ di_str;',
    'std::string opaque_str = "";\n    for(const auto& ot : opaque_types) { opaque_str += "%" + ot + " = type opaque\\n"; }\n    return opaque_str + "\\n" + ir_output.str() + "\\n" + global_strings_output.str() + di_str;',
    c
)

open(f, 'w').write(c)
