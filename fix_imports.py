with open('std/collections.alu', 'r') as f:
    s = f.read()
s = s.replace('import "std/mem.alu";', 'import std::mem;')
s = s.replace('import "std/string.alu";', 'import std::string;')
with open('std/collections.alu', 'w') as f:
    f.write(s)
