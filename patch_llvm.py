import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

c = c.replace(
    'return "%" + getNamespacedName(base) + "*";',
    'std::string n = getNamespacedName(base); opaque_types.insert(n); return "%" + n + "*";'
)

c = c.replace(
    'return "%" + getNamespacedName(clean_type) + "*";',
    'std::string n = getNamespacedName(clean_type); opaque_types.insert(n); return "%" + n + "*";'
)

# And now, output opaque types before writing the file!
# Where does it write the file? In writeOutput() or runClang()
# Let's find "Successfully wrote IR" or something.
c = c.replace(
    'out_file << ir_code;',
    'for(const auto& ot : opaque_types) { out_file << "%" << ot << " = type opaque\\n"; }\nout_file << ir_code;'
)

open(f, 'w').write(c)
