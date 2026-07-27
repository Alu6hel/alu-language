mod lexer;
mod ast;
mod verifier;
mod emitter;
mod lsp;
mod package_manager;
mod riscv_emitter;
mod jit;
mod ffi;
mod fpga_emitter;
mod hot_swap;
mod types;
mod aegis_vm;
mod android_jni;
mod windows_sys;

fn main() {
    println!("ALU Compiler: Initialization Complete.");
}
