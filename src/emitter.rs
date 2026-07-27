use crate::ast::ASTNode;

pub struct Emitter;

impl Emitter {
    pub fn new() -> Self {
        Self {}
    }

    pub fn walk_ast_to_c(&self, ast: &Vec<ASTNode>) -> String {
        let mut c_code = String::new();
        
        c_code.push_str("#include <stdio.h>\n");
        c_code.push_str("#include <stdbool.h>\n");
        c_code.push_str("#include <stdlib.h>\n\n");
        c_code.push_str("typedef unsigned char u8;\n");
        c_code.push_str("typedef unsigned long long u64;\n\n");
        
        // Hoare-logic stub
        c_code.push_str("bool local_heuristics_scan(u8* memory_ptr, u64 size) { return true; }\n\n");

        for node in ast {
            self.emit_node(node, &mut c_code, 0);
        }
        
        c_code
    }

    fn emit_node(&self, node: &ASTNode, c_code: &mut String, indent: usize) {
        let prefix = "    ".repeat(indent);
        match node {
            ASTNode::Routine { name, body } => {
                c_code.push_str(&format!("{}int {}() {{\n", prefix, name));
                for child in body {
                    self.emit_node(child, c_code, indent + 1);
                }
                c_code.push_str(&format!("{}    return 0;\n", prefix));
                c_code.push_str(&format!("{}}}\n", prefix));
            }
            ASTNode::Print { message } => {
                c_code.push_str(&format!("{}printf(\"{}\\n\");\n", prefix, message));
            }
            ASTNode::VariableDecl { name, value } => {
                // Simplified type inference
                if let ASTNode::Alloc { size } = &**value {
                    c_code.push_str(&format!("{}u8* {} = (u8*)malloc({});\n", prefix, name, size));
                } else if let ASTNode::Unknown { text } = &**value {
                    c_code.push_str(&format!("{}bool {} = {};\n", prefix, name, text));
                }
            }
            ASTNode::Loop { body } => {
                c_code.push_str(&format!("{}while (true) {{\n", prefix));
                for child in body {
                    self.emit_node(child, c_code, indent + 1);
                }
                c_code.push_str(&format!("{}    break; // Break for safety in transpiled demo\n", prefix));
                c_code.push_str(&format!("{}}}\n", prefix));
            }
            ASTNode::Condition { check, body } => {
                c_code.push_str(&format!("{}if ({}) {{\n", prefix, check));
                for child in body {
                    self.emit_node(child, c_code, indent + 1);
                }
                c_code.push_str(&format!("{}}}\n", prefix));
            }
            ASTNode::WriteJSON { file, status } => {
                c_code.push_str(&format!("{}FILE* f = fopen(\"{}\", \"w\");\n", prefix, file));
                c_code.push_str(&format!("{}if (f) {{ fprintf(f, \"{{\\\"status\\\": \\\"{}\\\"}}\\n\"); fclose(f); }}\n", prefix, status));
            }
            _ => {}
        }
    }
}

pub struct CfgNode;
pub struct RegisterAllocator;