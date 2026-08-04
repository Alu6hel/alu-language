import os
import re

def refactor_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    out_content = content
    
    # We will repeatedly replace innermost while loops
    # Regex 1: without type
    pattern1 = r'([a-zA-Z0-9_]+)\s*=\s*([^;]+);\s*while\s*\(([^)]+)\)\s*\{\s*(.*?)([a-zA-Z0-9_]+)\s*=\s*([^;]+);\s*\}'
    def repl1(m):
        if m.group(1) == m.group(5) and m.group(1) in m.group(3):
            return f"for ({m.group(1)} = {m.group(2)}; {m.group(3)}; {m.group(5)} = {m.group(6)}) {{ {m.group(4)} }}"
        return m.group(0)

    # Regex 2: with type
    pattern2 = r'(int|byte|float|string)\s+([a-zA-Z0-9_]+)\s*=\s*([^;]+);\s*while\s*\(([^)]+)\)\s*\{\s*(.*?)([a-zA-Z0-9_]+)\s*=\s*([^;]+);\s*\}'
    def repl2(m):
        if m.group(2) == m.group(6) and m.group(2) in m.group(4):
            return f"for ({m.group(1)} {m.group(2)} = {m.group(3)}; {m.group(4)}; {m.group(6)} = {m.group(7)}) {{ {m.group(5)} }}"
        return m.group(0)

    for _ in range(5): # run multiple passes for nested
        out_content = re.sub(pattern1, repl1, out_content, flags=re.DOTALL)
        out_content = re.sub(pattern2, repl2, out_content, flags=re.DOTALL)

    if out_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(out_content)
        print(f"Refactored {filepath}")

def main():
    for root, dirs, files in os.walk('.'):
        for file in files:
            if file.endswith('.alu') and file != "test_for_loops.alu" and "compiler" not in root:
                refactor_file(os.path.join(root, file))

if __name__ == "__main__":
    main()
