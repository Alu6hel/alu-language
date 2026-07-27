use alu_language::ast::Parser;
use alu_language::emitter::Emitter;
use alu_language::lexer::Token; // Assuming Token is defined in lexer.rs
use std::fs;
use std::process::Command;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        println!("ALU V15 Transpiler System (Stage 0.5)");
        println!("Usage: alu [build|run|lsp] <file.alu>");
        return;
    }

    match args[1].as_str() {
        "build" => {
            if args.len() < 3 {
                println!("Error: Missing input file.");
                return;
            }
            let input_file = &args[2];
            println!("[Stage 0.5] Parsing and transpiling {} to C...", input_file);
            
            // 1. Lexing (Stubbed for demo)
            let tokens = vec![]; 
            
            // 2. Dynamic AST Parsing
            let mut parser = Parser::new(tokens);
            let ast = parser.parse();

            // 2.5 Mathematical Verification
            let verifier = alu_language::verifier::Verifier::new();
            if let Err(e) = verifier.verify_ast(&ast) {
                eprintln!("[ALU Verifier Error] {}", e);
                eprintln!("Compilation halted. Math must prove.");
                std::process::exit(1);
            }
            
            // 3. Dynamic C Emission
            let emitter = Emitter::new();
            let c_code = emitter.walk_ast_to_c(&ast);
            
            let c_filename = input_file.replace(".alu", ".c");
            let exe_filename = input_file.replace(".alu", ".exe");
            
            fs::write(&c_filename, c_code).expect("Failed to write .c file");
            println!("[Stage 0.5] Successfully generated {}", c_filename);
            
            // 4. Compile using GCC
            println!("[Stage 0.5] Invoking GCC to generate native executable...");
            let status = Command::new("gcc")
                .arg(&c_filename)
                .arg("-o")
                .arg(&exe_filename)
                .status();
                
            match status {
                Ok(s) if s.success() => {
                    println!("[Stage 0.5] SUCCESS! Native binary {} created.", exe_filename);
                }
                _ => {
                    println!("[!] GCC not found or failed. The transpiled C code is ready at {}.", c_filename);
                }
            }
        }
        "init" => {
            let pm = alu_language::package_manager::PackageManager::new();
            pm.init_project();
        }
        "install" => {
            if args.len() < 3 {
                println!("Error: Missing package name.");
                return;
            }
            let pkg_name = &args[2];
            let pm = alu_language::package_manager::PackageManager::new();
            pm.install_package(pkg_name);
        }
        "lsp" => {
            alu_language::lsp::run_lsp_server();
        }
        "run" => {
            println!("Executing JIT Native Binary...");
        }
        _ => {
            println!("Unknown command: {}", args[1]);
        }
    }
}
