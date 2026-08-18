import sys

f = 'cpp_frontend/llvm_codegen.cpp'
c = open(f).read()

# Replace getNamespacedName in the final fallback
c = c.replace(
    'return "%" + getNamespacedName(type) + "*"; // Custom types are passed by reference',
    '{ std::string n = getNamespacedName(type); opaque_types.insert(n); return "%" + n + "*"; } // Custom types are passed by reference'
)

open(f, 'w').write(c)
