#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "z3_verifier.h"
#include "llvm_codegen.h"
#include "linker.h"

// Module Linking is now handled by ModuleLinker (linker.h)

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: alu_cxx <file.alu>" << std::endl;
        return 1;
    }

    std::vector<std::string> inputFiles;
    std::vector<std::string> extraLinkObjects;
    std::string command = "build";
    std::string targetTriple = "";
    bool createXcframework = false;
    std::string ndkPath = "";
    std::string jniPackage = "com.example.alu";
    std::string stdPath = ""; // Will be auto-detected if not set
    bool emit_debug_info = false;
    std::string opt_level = "-O3"; // Default optimization
    
    auto hasExtension = [](const std::string& path, const std::string& ext) {
        if (path.length() >= ext.length()) {
            return (0 == path.compare(path.length() - ext.length(), ext.length(), ext));
        }
        return false;
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--target=") == 0) {
            targetTriple = arg.substr(9);
        } else if (arg == "--create-xcframework") {
            createXcframework = true;
        } else if (arg == "--ndk-path" && i + 1 < argc) {
            ndkPath = argv[++i];
        } else if (arg == "--package" && i + 1 < argc) {
            jniPackage = argv[++i];
        } else if (arg == "--std-path" && i + 1 < argc) {
            stdPath = argv[++i];
        } else if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3" || arg == "-Os" || arg == "-Oz") {
            opt_level = arg;
        } else if (arg == "-g" || arg == "--debug") {
            emit_debug_info = true;
        } else if (i == 1 && (arg == "install" || arg == "build" || arg == "bindgen")) {
            command = arg;
        } else {
            if (hasExtension(arg, ".o") || hasExtension(arg, ".obj") || hasExtension(arg, ".a") || hasExtension(arg, ".lib") || hasExtension(arg, ".c")) {
                extraLinkObjects.push_back(arg);
            } else {
                inputFiles.push_back(arg);
            }
        }
    }
    
    // Check if a file exists and can be opened for reading
    auto fileExists = [](const std::string& path) {
        std::ifstream f(path);
        return f.good();
    };

    // Auto-detect std library path: try relative to the compiler executable's directory
    if (stdPath.empty()) {
        std::string exeDir = ModuleLinker::getDirectory(argv[0]);
        if (fileExists(exeDir + "/std/io.alu")) {
            stdPath = exeDir;
        } else if (fileExists(exeDir + "/../std/io.alu")) {
            stdPath = exeDir + "/..";
        } else {
            stdPath = "."; // fallback to current directory
        }
    }

    // Resolve target aliases
    if (targetTriple == "wasm32") targetTriple = "wasm32-unknown-unknown";
    else if (targetTriple == "arm64" || targetTriple == "aarch64") targetTriple = "aarch64-unknown-linux-gnu";
    else if (targetTriple == "android") targetTriple = "aarch64-linux-android";
    else if (targetTriple == "ios") targetTriple = "aarch64-apple-ios";
    else if (targetTriple == "ios-sim") targetTriple = "x86_64-apple-ios-simulator";
    else if (targetTriple == "x86_64") targetTriple = "x86_64-pc-windows-msvc";
    else if (targetTriple.empty()) targetTriple = "x86_64-pc-windows-msvc"; // Default

    bool targetAndroid = (targetTriple.find("-android") != std::string::npos);
    bool targetWasm = (targetTriple.find("wasm32") != std::string::npos);
    bool targetVulkan = (targetTriple.find("spirv") != std::string::npos);
    bool targetMetal = (targetTriple.find("air64") != std::string::npos);
    bool targetIos = (targetTriple == "aarch64-apple-ios");
    bool targetIosSim = (targetTriple.find("-ios-simulator") != std::string::npos);

    // XCFramework logic
    if (createXcframework) {
        if (inputFiles.empty()) {
            std::cerr << "Usage: alu_cxx build <file.alu> --create-xcframework" << std::endl;
            return 1;
        }
        std::string target = inputFiles[0];
        std::string baseFilename = target.substr(0, target.find_last_of("."));
        std::cout << "[ALU CXX] Creating XCFramework for " << baseFilename << "..." << std::endl;
        
        std::string exeName = argv[0];
        
        // 1. Build for iOS ARM64
        std::string cmdIos = exeName + " build " + target + " --target=aarch64-apple-ios";
        std::cout << "[ALU CXX] Executing: " << cmdIos << std::endl;
        if (std::system(cmdIos.c_str()) != 0) return 1;
        
        // 2. Build for iOS Simulator x86_64
        std::string cmdSimX86 = exeName + " build " + target + " --target=x86_64-apple-ios-simulator";
        std::cout << "[ALU CXX] Executing: " << cmdSimX86 << std::endl;
        if (std::system(cmdSimX86.c_str()) != 0) return 1;
        
        // 3. Build for iOS Simulator arm64
        std::string cmdSimArm = exeName + " build " + target + " --target=aarch64-apple-ios-simulator";
        std::cout << "[ALU CXX] Executing: " << cmdSimArm << std::endl;
        if (std::system(cmdSimArm.c_str()) != 0) return 1;
        
        // Combine simulator archs
        std::string cmdLipo = "lipo -create -output lib" + baseFilename + "-sim.a " + baseFilename + "-x86_64-apple-ios-simulator.a " + baseFilename + "-aarch64-apple-ios-simulator.a";
        std::system(cmdLipo.c_str());
        
        // Create xcframework
        std::string cmdRm = "rm -rf " + baseFilename + ".xcframework";
        std::system(cmdRm.c_str());
        
        std::string cmdXc = "xcodebuild -create-xcframework -library " + baseFilename + "-aarch64-apple-ios.a -library lib" + baseFilename + "-sim.a -output " + baseFilename + ".xcframework";
        int res = std::system(cmdXc.c_str());
        
        if (res == 0) {
            std::cout << "[ALU CXX] Successfully built " << baseFilename << ".xcframework!" << std::endl;
        }
        return res;
    }

    if (command == "install") {
        if (inputFiles.empty()) {
            std::cerr << "Usage: alu_cxx install <user/repo>" << std::endl;
            return 1;
        }
        std::string target = inputFiles[0];
        std::string url = "https://github.com/" + target;
        std::string repo_name = target;
        size_t slash_pos = target.find('/');
        if (slash_pos != std::string::npos) {
            repo_name = target.substr(slash_pos + 1);
        }
        std::string cmd = "git clone " + url + " alu_modules/" + repo_name;
        std::cout << "[ALU CXX] Installing package: " << target << "..." << std::endl;
        int res = system(cmd.c_str());
        if (res == 0) {
            std::cout << "[ALU CXX] Package installed successfully to alu_modules/" << repo_name << std::endl;
        } else {
            std::cerr << "[ALU CXX] Failed to install package." << std::endl;
            return 1;
        }
        return 0;
    }

    if (command == "bindgen") {
        std::string exeDir = ModuleLinker::getDirectory(argv[0]);
        std::string bindgenExe = exeDir + "/alu_bindgen.exe";
        if (!fileExists(bindgenExe)) {
            bindgenExe = "alu_bindgen";
        }
        std::string cmd = bindgenExe;
        for (int i = 2; i < argc; ++i) {
            cmd += " ";
            cmd += argv[i];
        }
        return std::system(cmd.c_str());
    }

    if (inputFiles.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
        return 1;
    }

    auto ast = std::make_unique<ProgramNode>();
    ModuleLinker linker(stdPath);

    for (const auto& filename : inputFiles) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        std::cout << "[ALU CXX] Compiling: " << filename << std::endl;
        std::cout << "[ALU CXX] Lexical Analysis (Scanning)..." << std::endl;
        
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        
        std::cout << "[ALU CXX] Syntactic Analysis (Parsing)..." << std::endl;
        
        Parser parser(tokens, filename);
        try {
            std::unique_ptr<ProgramNode> fileAst = parser.parse();
            
            // Resolve imports using ModuleLinker
            std::string sourceDir = ModuleLinker::getDirectory(filename);
            linker.link(fileAst.get(), sourceDir, filename);

            for (auto& decl : fileAst->declarations) {
                ast->declarations.push_back(std::move(decl));
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
        
        std::cout << "[ALU CXX] Abstract Syntax Tree generated successfully:" << std::endl;
        std::cout << "==================================================" << std::endl;
        ast->print();
        std::cout << "==================================================" << std::endl;
        
        try {
            SemanticAnalyzer semanticAnalyzer;
            semanticAnalyzer.analyze(ast.get());
            
            Z3Verifier z3Verifier;
            z3Verifier.verify(ast.get());
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
        
        std::cout << "[ALU CXX] Ready for LLVM IR Translation." << std::endl;
        
        LLVMCodeGen codegen(targetTriple);
        codegen.emit_debug_info = emit_debug_info;
        ast->codegen(codegen);
        std::string mainFile = inputFiles[0];
        std::string outFilename = mainFile + ".ll";
        codegen.saveToFile(outFilename);
        
        std::cout << "==================================================" << std::endl;
        std::cout << codegen.getIR();
        std::cout << "==================================================" << std::endl;
        std::cout << "[ALU CXX] Successfully wrote IR to " << outFilename << std::endl;
        
        // --- JNI BRIDGE GENERATION PHASE --- //
        if (targetAndroid) {
            std::cout << "[ALU CXX] Generating JNI Bridge for package: " << jniPackage << std::endl;
            std::ofstream jniFile("jni_bridge.cpp");
            jniFile << "#include <jni.h>\n";
            jniFile << "#include <string>\n";
            jniFile << "#include <vector>\n";
            jniFile << "#include <iostream>\n\n";
            jniFile << "extern \"C\" {\n";
            for (const auto& decl : ast->declarations) {
                if (auto rNode = dynamic_cast<RoutineNode*>(decl.get())) {
                    if (rNode->isExported) {
                        // Declare the ALU function extern so C++ can link to it
                        jniFile << "    extern int " << rNode->name << "(";
                        for (size_t i = 0; i < rNode->params.size(); ++i) {
                            if (rNode->params[i].type == "string" && rNode->params[i].name.find("buffer") != std::string::npos) {
                                jniFile << "char*";
                            } else if (rNode->params[i].type == "string" || rNode->params[i].type == "byte*" || rNode->params[i].type == "buffer") {
                                jniFile << "char*";
                            } else {
                                jniFile << "int";
                            }
                            if (i < rNode->params.size() - 1) jniFile << ", ";
                        }
                        jniFile << ");\n";
                    }
                }
            }
            jniFile << "}\n\n";
            
            std::string pkgUnderscore = jniPackage;
            for (char& c : pkgUnderscore) if (c == '.') c = '_';
            
            for (const auto& decl : ast->declarations) {
                if (auto rNode = dynamic_cast<RoutineNode*>(decl.get())) {
                    if (rNode->isExported) {
                        jniFile << "extern \"C\" JNIEXPORT jint JNICALL\n";
                        jniFile << "Java_" << pkgUnderscore << "_AluBridge_" << rNode->name << "(JNIEnv *env, jobject thiz";
                        for (size_t i = 0; i < rNode->params.size(); ++i) {
                            jniFile << ", ";
                            if (rNode->params[i].type == "string" && rNode->params[i].name.find("buffer") != std::string::npos) {
                                jniFile << "jobject " << rNode->params[i].name;
                            } else if (rNode->params[i].type == "string") {
                                jniFile << "jstring " << rNode->params[i].name;
                            } else if (rNode->params[i].type == "byte*" || rNode->params[i].type == "buffer") {
                                jniFile << "jobject " << rNode->params[i].name;
                            } else {
                                jniFile << "jint " << rNode->params[i].name;
                            }
                        }
                        jniFile << ") {\n";
                        
                        // Extract strings and arrays
                        std::vector<std::string> stringVars;
                        std::vector<std::string> arrayVars;
                        for (const auto& p : rNode->params) {
                            if (p.type == "string" && p.name.find("buffer") != std::string::npos) {
                                jniFile << "    void* c_" << p.name << " = env->GetDirectBufferAddress(" << p.name << ");\n";
                            } else if (p.type == "string") {
                                jniFile << "    const char* c_" << p.name << " = env->GetStringUTFChars(" << p.name << ", 0);\n";
                                stringVars.push_back(p.name);
                            } else if (p.type == "byte*" || p.type == "buffer") {
                                jniFile << "    void* c_" << p.name << " = env->GetDirectBufferAddress(" << p.name << ");\n";
                                // No release needed for direct buffer addresses
                            }
                        }
                        
                        jniFile << "    int result = " << rNode->name << "(";
                        for (size_t i = 0; i < rNode->params.size(); ++i) {
                            if (rNode->params[i].type == "string" || rNode->params[i].type == "byte*" || rNode->params[i].type == "buffer") {
                                jniFile << "(char*)c_" << rNode->params[i].name;
                            } else {
                                jniFile << rNode->params[i].name;
                            }
                            if (i < rNode->params.size() - 1) jniFile << ", ";
                        }
                        jniFile << ");\n";
                        
                        // Release strings
                        for (const auto& s : stringVars) {
                            jniFile << "    env->ReleaseStringUTFChars(" << s << ", c_" << s << ");\n";
                        }
                        
                        jniFile << "    return result;\n";
                        jniFile << "}\n\n";
                    }
                }
            }
            jniFile.close();
            std::cout << "[ALU CXX] JNI Bridge generated at jni_bridge.cpp" << std::endl;
        }

        // --- BACKEND LINKER PHASE --- //
        std::cout << "[ALU CXX] Invoking LLVM Backend (clang) to assemble and link..." << std::endl;
        
        // Strip the .alu extension and add .exe, .so, or .js
        std::string baseFilename = mainFile.substr(0, mainFile.find_last_of("."));
        std::string outBinFilename = baseFilename;
        
        if (targetAndroid) outBinFilename += ".so";
        else if (targetWasm) outBinFilename += ".wasm";
        else if (targetVulkan) outBinFilename += ".spv";
        else if (targetMetal) outBinFilename += ".metallib";
        else if (targetIos || targetIosSim) outBinFilename += "-" + targetTriple + ".a";
        else outBinFilename += ".exe";
        
        std::string compileCommand;
        if (targetWasm) {
            std::string clangWasmPath = "\"C:\\Program Files\\LLVM\\bin\\clang.exe\"";
            compileCommand = clangWasmPath + " --target=wasm32-unknown-unknown -O3 -nostdlib -Wl,--no-entry -Wl,--export-all -o " + outBinFilename + " " + outFilename;
        } else if (targetAndroid) {
            std::string resolvedNdk = ndkPath;
            if (resolvedNdk.empty()) {
                const char* envNdk = std::getenv("ANDROID_NDK_HOME");
                if (envNdk) {
                    resolvedNdk = envNdk;
                } else {
                    const char* localAppdata = std::getenv("LOCALAPPDATA");
                    if (localAppdata) {
                        resolvedNdk = std::string(localAppdata) + "\\Android\\Sdk\\ndk\\25.1.8937393"; // Default fallback
                    }
                }
            }
            std::string clangPath = resolvedNdk + "\\toolchains\\llvm\\prebuilt\\windows-x86_64\\bin\\aarch64-linux-android30-clang++";
            compileCommand = clangPath + " -shared -fPIC -o " + outBinFilename + " " + outFilename + " jni_bridge.cpp std/string_backend.cpp std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp -lcrypto -lssl";
        } else if (targetVulkan) {
            compileCommand = "llvm-spirv -o " + outBinFilename + " " + outFilename;
        } else if (targetMetal) {
            compileCommand = "xcrun -sdk macosx metal -c " + outFilename + " -o " + baseFilename + ".air && xcrun -sdk macosx metallib " + baseFilename + ".air -o " + outBinFilename;
        } else if (targetIos) {
            std::string objFile = baseFilename + "-arm64.o";
#ifdef _WIN32
            compileCommand = "clang -x ir " + outFilename + " -c -O3 -target aarch64-apple-ios -o " + objFile;
            outBinFilename = objFile;
#else
            compileCommand = "clang -x ir " + outFilename + " std/string_backend.cpp std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp std/packet_backend.cpp -c -O3 -arch arm64 -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -miphoneos-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
#endif
        } else if (targetIosSim) {
            std::string arch = (targetTriple == "aarch64-apple-ios-simulator") ? "arm64" : "x86_64";
            std::string objFile = baseFilename + "-" + arch + "-sim.o";
#ifdef _WIN32
            compileCommand = "clang -x ir " + outFilename + " -c -O3 -target " + targetTriple + " -o " + objFile;
            outBinFilename = objFile;
#else
            compileCommand = "clang -x ir " + outFilename + " std/string_backend.cpp std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp std/packet_backend.cpp -c -O3 -arch " + arch + " -isysroot $(xcrun --sdk iphonesimulator --show-sdk-path) -mios-simulator-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
#endif
        } else {
            std::string opt_flag = emit_debug_info ? "-O0 -g" : opt_level;
            compileCommand = "clang++ " + opt_flag + " -o " + outBinFilename + " " + outFilename + " \"" + stdPath + "/std/fs_backend.cpp\" \"" + stdPath + "/std/net_backend.cpp\" \"" + stdPath + "/std/crypto_backend.cpp\" \"" + stdPath + "/std/string_backend.cpp\" \"" + stdPath + "/std/image_backend.cpp\" \"" + stdPath + "/std/thread_backend.cpp\" -lws2_32";
            for (size_t i = 1; i < inputFiles.size(); ++i) {
                compileCommand += " \"" + inputFiles[i] + "\"";
            }
            for (const auto& obj : extraLinkObjects) {
                compileCommand += " \"" + obj + "\"";
            }
        }
        
        int result = std::system(compileCommand.c_str());
        
        if (result == 0) {
            std::cout << "[ALU CXX] Compilation Successful! Binary built at: " << outBinFilename << std::endl;
        } else {
            std::cerr << "[ALU CXX] Linker Error: Clang failed with exit code " << result << std::endl;
            return 1;
        }
        
    return 0;
}
