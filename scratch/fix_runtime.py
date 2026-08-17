import sys

with open('alu_runtime.h', 'r') as f:
    content = f.read()

replacement = '''    void* alu_alloc(size_t size) {
        return malloc(size);
    }
    
    void* __alu_alloc_internal(int32_t size) {
        return malloc(size);
    }'''

content = content.replace('    void* alu_alloc(size_t size) {\n        return malloc(size);\n    }', replacement)

with open('alu_runtime.h', 'w') as f:
    f.write(content)
