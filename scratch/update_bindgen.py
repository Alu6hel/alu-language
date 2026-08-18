import re

with open('alu_bindgen.py', 'r', encoding='utf-8') as f:
    lines = f.read()

pass1_code = """
    # Pass 1: Real definitions
    for node in tu.cursor.get_children():
        if node.location.file and node.location.file.name != tu.spelling:
            continue
            
        if node.kind == CursorKind.STRUCT_DECL or node.kind == CursorKind.UNION_DECL:
            name = node.spelling
            if not name:
                name = node.type.spelling.replace("struct ", "").replace("union ", "").strip()
            
            if name and name not in parsed_structs:
                fields = list(node.get_children())
                if fields:
                    parsed_structs.add(name)
                    out.append(f"struct {name} {{")
                    for field in fields:
                        if field.kind == CursorKind.FIELD_DECL:
                            alu_type = map_type(field.type)
                            out.append(f"    {alu_type} {field.spelling} ;")
                    out.append("}")
                    out.append("")
                    
        elif node.kind == CursorKind.TYPEDEF_DECL:
            underlying = node.underlying_typedef_type.get_canonical()
            if underlying.kind == TypeKind.RECORD:
                name = node.spelling
                if name not in parsed_structs:
                    decl = underlying.get_declaration()
                    fields = list(decl.get_children())
                    if fields:
                        parsed_structs.add(name)
                        out.append(f"struct {name} {{")
                        for field in fields:
                            if field.kind == CursorKind.FIELD_DECL:
                                alu_type = map_type(field.type)
                                out.append(f"    {alu_type} {field.spelling} ;")
                        out.append("}")
                        out.append("")

    # Pass 2: Opaque handles and Functions
    for node in tu.cursor.get_children():
        if node.location.file and node.location.file.name != tu.spelling:
            continue
            
        if node.kind == CursorKind.STRUCT_DECL or node.kind == CursorKind.UNION_DECL:
            name = node.spelling
            if not name:
                name = node.type.spelling.replace("struct ", "").replace("union ", "").strip()
            if name and name not in parsed_structs:
                parsed_structs.add(name)
                out.append(f"struct {name} {{")
                out.append(f"    int opaque_dummy_field ;")
                out.append("}")
                out.append("")
                
        elif node.kind == CursorKind.FUNCTION_DECL:
"""

new_script = re.sub(r'    def visit\(node\):.*for child in tu.cursor.get_children\(\):\n\s*visit\(child\)', pass1_code + """
            name = node.spelling
            if name not in parsed_funcs:
                parsed_funcs.add(name)
                ret_type = map_type(node.result_type)
                
                params = []
                for idx, arg in enumerate(node.get_arguments()):
                    arg_type = map_type(arg.type)
                    arg_name = arg.spelling if arg.spelling else f"arg{idx}"
                    params.append(f"{arg_type} {arg_name}")
                
                params_str = " , ".join(params)
                if params_str:
                    out.append(f"extern routine {name} ( {params_str} ) -> {ret_type} ;")
                else:
                    out.append(f"extern routine {name} ( ) -> {ret_type} ;")
""", lines, flags=re.DOTALL)

with open('alu_bindgen.py', 'w', encoding='utf-8') as f:
    f.write(new_script)
