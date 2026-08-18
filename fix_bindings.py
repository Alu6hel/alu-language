import re

def fix_bindings(filename):
    with open(filename, 'r') as f:
        content = f.read()
    
    # 1. fix >>
    content = content.replace('>>', '> >')
    
    # 2. fix string variable name
    content = re.sub(r'\bstring\b(?=[\,\)])', 'str_val', content)
    
    # 3. remove calling conventions
    for mac in ['GL_APICALL ', ' GL_APIENTRY', 'VKAPI_ATTR ', ' VKAPI_CALL', 'VKAPI_PTR ']:
        content = content.replace(mac, '')
        
    # 4. fix empty structs
    content = content.replace('struct struct {', 'struct OpaqueStruct {')
    
    # 5. fix pointer structs
    content = re.sub(r'struct ([a-zA-Z0-9_]+)\*\s*\{', r'struct \1 {', content)
    
    # 6. fix broken vulkan functions
    # extern routine VkResult(ptr<*PFN_vkCreateInstance)( VkInstanceCreateInfo> pCreateInfo, ptr<VkAllocationCallbacks> pAllocator, VkInstance* pInstance);
    # -> extern routine vkCreateInstance(VkInstanceCreateInfo* pCreateInfo, VkAllocationCallbacks* pAllocator, VkInstance* pInstance) -> VkResult;
    # wait, the parameter might have a stray '>' like `VkInstanceCreateInfo>`. Let's fix ptr< type
    
    lines = content.split('\n')
    new_lines = []
    for line in lines:
        if line.startswith('typedef '):
            continue
        
        if line.startswith('extern routine '):
            # Try to match broken function pointer declarations
            m = re.match(r'extern routine ([a-zA-Z0-9_]+)\(ptr<\*PFN_([a-zA-Z0-9_]+)\)\(\s*(.*)\);', line)
            if m:
                retType = m.group(1)
                name = m.group(2)
                args = m.group(3)
                # Fix stray `>` in args like `VkInstanceCreateInfo>` 
                args = re.sub(r'([a-zA-Z0-9_]+)>', r'ptr<\1>', args)
                new_lines.append(f'extern routine {name}({args}) -> {retType};')
                continue
                
            m2 = re.match(r'extern routine ([a-zA-Z0-9_]+)\(\*PFN_([a-zA-Z0-9_]+)\)\(\s*(.*)\);', line)
            if m2:
                retType = m2.group(1)
                name = m2.group(2)
                args = m2.group(3)
                args = re.sub(r'([a-zA-Z0-9_]+)>', r'ptr<\1>', args)
                new_lines.append(f'extern routine {name}({args}) -> {retType};')
                continue

        new_lines.append(line)
        
    with open(filename, 'w') as f:
        f.write('\n'.join(new_lines))

fix_bindings('std/gles3.alu')
fix_bindings('std/vulkan.alu')
