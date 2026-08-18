import os

def clean_file(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
        
    out = []
    in_struct = False
    
    for l in lines:
        if l.startswith('struct '):
            in_struct = True
            out.append(l)
            continue
            
        if in_struct:
            out.append(l)
            if l.startswith('}'):
                in_struct = False
            continue
            
        if l.startswith('extern ') or l.startswith('namespace ') or l.startswith('routine ') or l.startswith('//') or l.startswith('}') or l.strip() == '':
            out.append(l)
            
    with open(filename, 'w') as f:
        f.writelines(out)

clean_file('std/gles3.alu')
clean_file('std/vulkan.alu')
