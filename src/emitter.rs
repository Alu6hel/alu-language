use crate::ast::ASTNode;

pub struct Emitter {
    global_strings: std::cell::RefCell<Vec<String>>,
}

impl Emitter {
    pub fn new() -> Self {
        Self {
            global_strings: std::cell::RefCell::new(Vec::new()),
        }
    }

    fn allocate_string(&self, s: &str) -> usize {
        let mut globals = self.global_strings.borrow_mut();
        let idx = globals.len();
        globals.push(s.to_string());
        idx
    }

    pub fn generate_llvm_ir(&self, ast: &Vec<ASTNode>) -> String {
        let mut ir = String::new();
        
        // Target Configuration (ALU Mobile Support)
        ir.push_str("target triple = \"aarch64-unknown-linux-android\"\n\n");

        // Headers
        ir.push_str("declare i32 @puts(ptr)\n");
        ir.push_str("declare ptr @malloc(i64)\n\n");
        
        // Functions
        let mut functions_ir = String::new();
        let mut label_counter = 0;
        
        for node in ast {
            functions_ir.push_str(&self.emit_node(node, &mut label_counter));
        }
        
        // Globals
        for (i, s) in self.global_strings.borrow().iter().enumerate() {
            // Convert to LLVM string format: add null terminator \00
            let mut llvm_str = String::new();
            for c in s.chars() {
                if c == '\\' { llvm_str.push_str("\\\\"); }
                else if c == '"' { llvm_str.push_str("\\22"); }
                else { llvm_str.push(c); }
            }
            llvm_str.push_str("\\00");
            let len = llvm_str.len() - llvm_str.matches("\\").count() * 2; // actual byte length
            // A bit tricky to get exact byte length, let's just do a simple ascii len + 1
            let byte_len = s.len() + 1;
            ir.push_str(&format!("@.str.{} = private unnamed_addr constant [{} x i8] c\"{}\"\n", i, byte_len, llvm_str));
        }
        ir.push_str("\n");
        
        ir.push_str(&functions_ir);
        ir
    }

    fn emit_node(&self, node: &ASTNode, label_counter: &mut usize) -> String {
        let mut ir = String::new();
        match node {
            ASTNode::Import { path } => {
                ir.push_str(&format!("  ; import {}\n", path));
            }
            ASTNode::Routine { name, requires: _, ensures: _, body } => {
                ir.push_str(&format!("define i32 @{}() {{\nentry:\n", name));
                for child in body {
                    ir.push_str(&self.emit_node(child, label_counter));
                }
                ir.push_str("  ret i32 0\n}\n");
            }
            ASTNode::Print { message } => {
                let idx = self.allocate_string(message);
                ir.push_str(&format!("  call i32 @puts(ptr @.str.{})\n", idx));
            }
            ASTNode::VariableDecl { name, value } => {
                ir.push_str(&format!("  %{} = alloca ptr\n", name)); // simplistic, assume all ptrs
                let val_ir = self.emit_node(value, label_counter);
                // The value node should return the IR to compute the value and the result register, but for now we simplify:
                ir.push_str(&val_ir);
                if let ASTNode::Alloc { size } = &**value {
                    ir.push_str(&format!("  store ptr %alloc_res_{}, ptr %{}\n", size, name));
                } else if let ASTNode::Unknown { text } = &**value {
                    if text.starts_with("CALL ") {
                        let target = text.trim_start_matches("CALL ");
                        ir.push_str(&format!("  %call_res_{} = call i32 @{}()\n", name, target));
                        // Since alloca was for ptr, we just simulate storing it
                        ir.push_str(&format!("  ; (Mock store i32 to ptr) store i32 %call_res_{}, ptr %{}\n", name, name));
                    } else {
                        ir.push_str(&format!("  ; (Unknown expr {}) store result to %{}\n", text, name));
                    }
                }
            }
            ASTNode::Alloc { size } => {
                ir.push_str(&format!("  %alloc_res_{} = call ptr @malloc(i64 {})\n", size, size));
            }
            ASTNode::Loop { body } => {
                let l_start = *label_counter;
                let l_end = *label_counter + 1;
                *label_counter += 2;
                ir.push_str(&format!("  br label %loop_start_{}\nloop_start_{}:\n", l_start, l_start));
                for child in body {
                    ir.push_str(&self.emit_node(child, label_counter));
                }
                ir.push_str(&format!("  br label %loop_start_{}\nloop_end_{}:\n", l_start, l_end));
            }
            ASTNode::Condition { check, body } => {
                let l_true = *label_counter;
                let l_end = *label_counter + 1;
                *label_counter += 2;
                // Mock condition check
                ir.push_str(&format!("  ; mock condition {}\n  br i1 1, label %cond_true_{}, label %cond_end_{}\ncond_true_{}:\n", check, l_true, l_end, l_true));
                for child in body {
                    ir.push_str(&self.emit_node(child, label_counter));
                }
                ir.push_str(&format!("  br label %cond_end_{}\ncond_end_{}:\n", l_end, l_end));
            }
            ASTNode::WriteJSON { file, status } => {
                let msg = format!("Writing {} to {}", status, file);
                let idx = self.allocate_string(&msg);
                ir.push_str(&format!("  call i32 @puts(ptr @.str.{})\n", idx));
            }
            ASTNode::Return { value } => {
                ir.push_str(&format!("  ret i32 {}\n", value));
            }
            ASTNode::Spawn { thread_id, body } => {
                ir.push_str(&format!("; spawn thread {}\n", thread_id));
                for child in body {
                    ir.push_str(&self.emit_node(child, label_counter));
                }
            }
            ASTNode::Lock { mutex_name } => {
                ir.push_str(&format!("; lock mutex {}\n", mutex_name));
            }
            ASTNode::Unknown { text } => {
                ir.push_str(&format!("; WARNING: Unrecognized instruction '{}'\n", text));
            }
        }
        ir
    }
}

pub struct CfgNode;
pub struct RegisterAllocator;