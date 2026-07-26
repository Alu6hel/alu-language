use std::collections::HashMap;

/// Slice 8: Control Flow Graph (CFG) & SSA Form
#[derive(Debug, PartialEq)]
pub enum SsaInstruction {
    Assign { var: String, val: i64 },
    MemorySafeRead { dest_var: String, cap_base: String, offset: i64 },
}

pub struct CfgNode {
    pub instructions: Vec<SsaInstruction>,
}

/// Slice 9: Register Allocation (Graph Coloring)
pub struct RegisterAllocator {
    pub allocation: HashMap<String, String>, // Maps var name to e.g. "rax"
}

impl RegisterAllocator {
    pub fn new() -> Self {
        RegisterAllocator {
            allocation: HashMap::new(),
        }
    }

    pub fn allocate(&mut self, cfg: &CfgNode) {
        let registers = vec!["rax", "rbx", "rcx", "rdx"];
        let mut reg_idx = 0;

        for instr in &cfg.instructions {
            match instr {
                SsaInstruction::Assign { var, .. } | SsaInstruction::MemorySafeRead { dest_var: var, .. } => {
                    if !self.allocation.contains_key(var) {
                        self.allocation.insert(var.clone(), registers[reg_idx % registers.len()].to_string());
                        reg_idx += 1;
                    }
                }
            }
        }
    }
}

/// Slice 10: Target Emitter
pub struct X86Emitter;

impl X86Emitter {
    pub fn emit(cfg: &CfgNode, allocator: &RegisterAllocator) -> String {
        let mut assembly = String::new();
        assembly.push_str(".text\n.global _start\n_start:\n");

        for instr in &cfg.instructions {
            match instr {
                SsaInstruction::Assign { var, val } => {
                    let reg = allocator.allocation.get(var).unwrap_or(&"r8".to_string()).clone();
                    assembly.push_str(&format!("  mov {}, {}\n", reg, val));
                }
                SsaInstruction::MemorySafeRead { dest_var, cap_base, offset } => {
                    let reg = allocator.allocation.get(dest_var).unwrap_or(&"r8".to_string()).clone();
                    // Simulated memory safe read macro/inline assembly
                    assembly.push_str(&format!("  ; memory_safe_read from {} + {}\n", cap_base, offset));
                    assembly.push_str(&format!("  mov {}, [{} + {}]\n", reg, cap_base, offset));
                }
            }
        }
        
        assembly
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_slice_8_9_10() {
        let cfg = CfgNode {
            instructions: vec![
                SsaInstruction::Assign { var: "v0".to_string(), val: 42 },
                SsaInstruction::MemorySafeRead {
                    dest_var: "v1".to_string(),
                    cap_base: "vga_buf".to_string(),
                    offset: 0,
                },
            ],
        };

        let mut allocator = RegisterAllocator::new();
        allocator.allocate(&cfg);
        
        assert_eq!(allocator.allocation.get("v0").unwrap(), "rax");
        assert_eq!(allocator.allocation.get("v1").unwrap(), "rbx");

        let asm = X86Emitter::emit(&cfg, &allocator);
        assert!(asm.contains("mov rax, 42"));
        assert!(asm.contains("memory_safe_read"));
        assert!(asm.contains("mov rbx, [vga_buf + 0]"));
    }
}
