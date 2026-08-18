import sys
import os
import argparse
import clang.cindex
from clang.cindex import CursorKind, TypeKind

# Alu type mapping
TYPE_MAP = {
    TypeKind.VOID: "void",
    TypeKind.BOOL: "bool",
    TypeKind.CHAR_U: "byte",
    TypeKind.UCHAR: "byte",
    TypeKind.CHAR16: "int",
    TypeKind.CHAR32: "int",
    TypeKind.USHORT: "int",
    TypeKind.UINT: "int",
    TypeKind.ULONG: "int",
    TypeKind.ULONGLONG: "int",
    TypeKind.CHAR_S: "byte",
    TypeKind.SCHAR: "byte",
    TypeKind.WCHAR: "int",
    TypeKind.SHORT: "int",
    TypeKind.INT: "int",
    TypeKind.LONG: "int",
    TypeKind.LONGLONG: "int",
    TypeKind.FLOAT: "float",
    TypeKind.DOUBLE: "double",
}

def map_type(clang_type):
    # Strip const/volatile
    canonical = clang_type.get_canonical()
    
    if canonical.kind == TypeKind.POINTER:
        pointee = canonical.get_pointee()
        # string special cases
        if pointee.kind in [TypeKind.CHAR_S, TypeKind.SCHAR, TypeKind.CHAR_U, TypeKind.UCHAR]:
            return "string"
        elif pointee.kind == TypeKind.VOID:
            return "ptr<byte>" # Alu often uses ptr<byte> for void*
        else:
            base_alu = map_type(pointee)
            if base_alu:
                return f"ptr< {base_alu} >"
            return "ptr< byte >"
    
    elif canonical.kind == TypeKind.RECORD:
        return canonical.spelling.replace("struct ", "").replace("union ", "").replace("const ", "").strip()
    
    elif canonical.kind == TypeKind.ENUM:
        return "int"
    
    elif canonical.kind == TypeKind.CONSTANTARRAY or canonical.kind == TypeKind.INCOMPLETEARRAY:
        elem_type = map_type(canonical.element_type)
        return f"ptr< {elem_type} >"
        
    elif canonical.kind in TYPE_MAP:
        return TYPE_MAP[canonical.kind]
    
    return "int" # fallback

def parse_header(header_path, output_path):
    index = clang.cindex.Index.create()
    tu = index.parse(header_path, args=['-x', 'c'])

    out = []
    out.append(f"// Auto-generated bindings for {os.path.basename(header_path)}")
    out.append("")
    
    parsed_structs = set()
    parsed_funcs = set()
    

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

        
    with open(output_path, "w") as f:
        f.write("\n".join(out))
    
    print(f"[ALU BINDGEN] Generated bindings -> {output_path}")

def main():
    parser = argparse.ArgumentParser(description="Alu FFI Bindgen")
    parser.add_argument("input", help="Input C header file")
    parser.add_argument("-o", "--output", help="Output .alu file", required=True)
    
    args = parser.parse_args()
    parse_header(args.input, args.output)

if __name__ == "__main__":
    main()
