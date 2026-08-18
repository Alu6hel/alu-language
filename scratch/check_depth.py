import re

text = open('std/vulkan.alu', encoding='utf-8').read()
# Remove comments
text = re.sub(r'//.*', '', text)
text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)

tokens = re.findall(r'->|<=|>=|<|>', text)

depth = 0
for t in tokens:
    if t == '<':
        depth += 1
    elif t == '>':
        depth -= 1
        if depth < 0:
            print("Negative depth!")

print("Final depth:", depth)
