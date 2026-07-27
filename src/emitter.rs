use std::collections::HashMap;

/// Slice 8: Control Flow Graph (CFG) & SSA Form
#[derive(Debug, PartialEq)]
pub enum SsaInstruction {
    Assign {
        var: String,
        val: i64,
    },
    MemorySafeRead {
        dest_var: String,
        cap_base: String,
        offset: i64,
    },
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

    pub fn emit_elf_so_format(machine_code: &[u8]) -> Vec<u8> {
        let mut elf = Vec::new();
        // TinyELF Header for AMD64
        elf.extend_from_slice(&[
            0x7F, 0x45, 0x4C, 0x46, // Magic: 0x7F 'E' 'L' 'F'
            0x02, // 64-bit
            0x01, // Little Endian
            0x01, // Version 1
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Padding
            0x03, 0x00, // e_type: ET_DYN (Shared object file)
            0x3E, 0x00, // e_machine: EM_X86_64
            0x01, 0x00, 0x00, 0x00, // e_version: 1
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // e_entry: Entry point
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // e_phoff: Program header offset
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // e_shoff: Section header offset
            0x00, 0x00, 0x00, 0x00, // e_flags
            0x40, 0x00, // e_ehsize
            0x38, 0x00, // e_phentsize
            0x01, 0x00, // e_phnum
            0x00, 0x00, // e_shentsize
            0x00, 0x00, // e_shnum
            0x00, 0x00, // e_shstrndx
        ]);
        
        // PT_LOAD Program Header
        elf.extend_from_slice(&[
            0x01, 0x00, 0x00, 0x00, // p_type: PT_LOAD
            0x05, 0x00, 0x00, 0x00, // p_flags: PF_R | PF_X
            0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // p_offset
            0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // p_vaddr
            0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // p_paddr
        ]);
        // Size padding for the payload length
        let size = machine_code.len() as u64;
        elf.extend_from_slice(&size.to_le_bytes()); // p_filesz
        elf.extend_from_slice(&size.to_le_bytes()); // p_memsz
        elf.extend_from_slice(&[0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]); // p_align: 4096
        
        elf.extend_from_slice(machine_code);
        elf
    }

    pub fn emit_pe_sys_format(machine_code: &[u8]) -> Vec<u8> {
        let mut pe = Vec::new();
        // TinyPE MS-DOS Stub
        pe.extend_from_slice(&[
            0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, // 'M' 'Z'
            0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
            0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, // e_lfanew
        ]);
        // PE Signature
        pe.extend_from_slice(&[0x50, 0x45, 0x00, 0x00]); // 'P' 'E' '\0' '\0'
        // COFF Header (AMD64)
        pe.extend_from_slice(&[
            0x64, 0x86, // Machine: AMD64
            0x01, 0x00, // NumberOfSections: 1
            0x00, 0x00, 0x00, 0x00, // TimeDateStamp
            0x00, 0x00, 0x00, 0x00, // PointerToSymbolTable
            0x00, 0x00, 0x00, 0x00, // NumberOfSymbols
            0xF0, 0x00, // SizeOfOptionalHeader
            0x22, 0x20, // Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE | DLL
        ]);
        
        // Optional Header (PE32+)
        pe.extend_from_slice(&[
            0x0B, 0x02, // Magic: PE32+
            0x00, 0x00, // LinkerVersion
            0x00, 0x00, 0x00, 0x00, // SizeOfCode
            0x00, 0x00, 0x00, 0x00, // SizeOfInitializedData
            0x00, 0x00, 0x00, 0x00, // SizeOfUninitializedData
            0x00, 0x10, 0x00, 0x00, // AddressOfEntryPoint
            0x00, 0x10, 0x00, 0x00, // BaseOfCode
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ImageBase
            0x00, 0x10, 0x00, 0x00, // SectionAlignment: 4096
            0x00, 0x02, 0x00, 0x00, // FileAlignment: 512
        ]);
        
        // Pad out to machine_code offset
        let padding = vec![0x00; 120];
        pe.extend_from_slice(&padding);
        
        pe.extend_from_slice(machine_code);
        pe
    }

    pub fn allocate(&mut self, cfg: &CfgNode) {
        let registers = vec!["rax", "rbx", "rcx", "rdx"];
        let mut reg_idx = 0;

        for instr in &cfg.instructions {
            match instr {
                SsaInstruction::Assign { var, .. }
                | SsaInstruction::MemorySafeRead { dest_var: var, .. } => {
                    if !self.allocation.contains_key(var) {
                        self.allocation.insert(
                            var.clone(),
                            registers[reg_idx % registers.len()].to_string(),
                        );
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
                    let reg = allocator
                        .allocation
                        .get(var)
                        .unwrap_or(&"r8".to_string())
                        .clone();
                    assembly.push_str(&format!("  mov {}, {}\n", reg, val));
                }
                SsaInstruction::MemorySafeRead {
                    dest_var,
                    cap_base,
                    offset,
                } => {
                    let reg = allocator
                        .allocation
                        .get(dest_var)
                        .unwrap_or(&"r8".to_string())
                        .clone();
                    // Simulated memory safe read macro/inline assembly
                    assembly.push_str(&format!(
                        "  ; memory_safe_read from {} + {}\n",
                        cap_base, offset
                    ));
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
                SsaInstruction::Assign {
                    var: "v0".to_string(),
                    val: 42,
                },
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
