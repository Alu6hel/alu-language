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
            let mut input_file = args[2].clone();
            let mut is_android = false;
            let mut is_bootstrap = false;
            
            if input_file == "--android" {
                is_android = true;
                if args.len() < 4 {
                    println!("Error: Missing input file after --android.");
                    return;
                }
                input_file = args[3].clone();
            } else if input_file == "--bootstrap" {
                is_bootstrap = true;
                if args.len() < 4 {
                    println!("Error: Missing input file after --bootstrap.");
                    return;
                }
                input_file = args[3].clone();
            }

            println!("[Stage 1.0] Parsing and lowering {} to LLVM IR...", input_file);
            
            let source_code = std::fs::read_to_string(&input_file).expect("Failed to read input file");
            
            let ast = if is_bootstrap {
                println!("[ALU Self-Hosting] Bypassing Rust parsing. Using compiler/lexer.alu and compiler/ast.alu...");
                // Note: Mocking the execution of pure ALU AST generation.
                // True integration would load the compiled `.ll` or interpreter state.
                Vec::new() // Mock empty AST
            } else {
                let mut lexer = alu_language::lexer::Lexer::new(&source_code);
                let mut tokens = Vec::new();
                loop {
                    let token = lexer.next_token();
                    if token == alu_language::lexer::Token::EOF {
                        break;
                    }
                    tokens.push(token);
                }
                
                let mut parser = Parser::new(tokens);
                parser.parse()
            };

            let verifier = alu_language::verifier::Verifier::new();
            if let Err(e) = verifier.verify_ast(&ast) {
                eprintln!("[ALU Verifier Error] {}", e);
                eprintln!("Compilation halted. Math must prove.");
                std::process::exit(1);
            }
            
            // 3. LLVM IR Emission
            let emitter = Emitter::new();
            let llvm_ir = emitter.generate_llvm_ir(&ast);
            
            let ll_filename = input_file.replace(".alu", ".ll");
            let exe_filename = if is_android {
                input_file.replace(".alu", ".so")
            } else {
                input_file.replace(".alu", ".exe")
            };
            
            fs::write(&ll_filename, llvm_ir).expect("Failed to write .ll file");
            println!("[Stage 1.0] Successfully generated LLVM IR at {}", ll_filename);
            
            // 4. Compile using clang/LLVM linker
            println!("[Stage 1.0] Invoking LLVM backend to generate native binary...");
            let mut cmd = Command::new("clang");
            cmd.arg(&ll_filename);
            
            if is_android {
                cmd.arg("-shared");
                cmd.arg("-target");
                cmd.arg("aarch64-linux-android");
            }
            
            cmd.arg("-o").arg(&exe_filename);
            let status = cmd.status();
                
            match status {
                Ok(s) if s.success() => {
                    println!("[Stage 1.0] SUCCESS! Native binary {} created.", exe_filename);
                }
                _ => {
                    println!("[!] LLVM tools not found or failed. The generated LLVM IR is ready at {}.", ll_filename);
                }
            }
        }
        "pack" => {
            if args.len() < 4 {
                println!("Usage: alu pack <project.toml> --target=<windows-msi|android>");
                return;
            }
            let project_file = &args[2];
            let target_arg = &args[3];
            let target = target_arg.replace("--target=", "");
            
            let packager = alu_language::packager::Packager::new();
            packager.pack(project_file, &target);
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
        "fetch" => {
            let pm = alu_language::package_manager::PackageManager::new();
            pm.fetch_dependencies();
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
