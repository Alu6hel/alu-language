import os
import re

def refactor_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    out_lines = []
    i = 0
    changed = False
    
    while i < len(lines):
        line = lines[i]
        
        # Look for while loop
        m_while = re.search(r'^(\s*)while\s*\((.*?)\)\s*\{\s*$', line)
        if m_while and i > 0:
            indent = m_while.group(1)
            cond = m_while.group(2)
            
            # Check if previous line is a variable declaration
            prev_line = out_lines[-1] if out_lines else ""
            m_decl = re.search(r'^(\s*)(?:(int|byte|float|string)\s+)?([a-zA-Z0-9_]+)\s*=\s*(.*?);\s*$', prev_line)
            
            if m_decl and m_decl.group(1) == indent:
                var_type = m_decl.group(2) if m_decl.group(2) else ""
                var_name = m_decl.group(3)
                init_val = m_decl.group(4)
                
                # Check if the condition uses the variable
                # Just assuming it's a standard loop if it matches this pattern
                
                # We need to find the matching closing brace and the update statement
                j = i + 1
                brace_count = 1
                update_line_idx = -1
                update_stmt = ""
                
                while j < len(lines):
                    if '{' in lines[j]:
                        brace_count += lines[j].count('{')
                    if '}' in lines[j]:
                        brace_count -= lines[j].count('}')
                    
                    if brace_count == 0:
                        # Found the end of the while loop
                        # Look at the line right before the closing brace (ignoring empty lines)
                        k = j - 1
                        while k > i and lines[k].strip() == "":
                            k -= 1
                        
                        m_update = re.search(r'^\s*(' + re.escape(var_name) + r'\s*=\s*.*?);\s*$', lines[k])
                        if m_update:
                            update_stmt = m_update.group(1)
                            update_line_idx = k
                        break
                    j += 1
                
                if update_line_idx != -1:
                    # We can convert this!
                    out_lines.pop() # Remove the previous declaration line
                    type_str = f"{var_type} " if var_type else ""
                    for_stmt = f"{indent}for ({type_str}{var_name} = {init_val}; {cond}; {update_stmt}) {{\n"
                    out_lines.append(for_stmt)
                    
                    # Add body
                    for k in range(i + 1, j):
                        if k != update_line_idx:
                            out_lines.append(lines[k])
                    
                    out_lines.append(lines[j]) # closing brace
                    i = j + 1
                    changed = True
                    continue
        
        out_lines.append(line)
        i += 1
        
    if changed:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.writelines(out_lines)
        print(f"Refactored {filepath}")

def main():
    for root, dirs, files in os.walk('.'):
        for file in files:
            if file.endswith('.alu') and file != "test_for_loops.alu":
                refactor_file(os.path.join(root, file))

if __name__ == "__main__":
    main()
