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

// --- Module Resolution ---

// Get the directory portion of a file path (everything before the last / or \)
static std::string getDirectory(const std::string& filepath) {
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash == std::string::npos) return ".";
    return filepath.substr(0, lastSlash);
}

// Check if a file exists and can be opened for reading
static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Resolve a module path to a filesystem path.
// For module-path imports (std::fs), converts :: to / and appends .alu.
// Search order:
//   1. std library dir (for std:: prefix)
//   2. Relative to the source file directory
//   3. alu_modules/ directory
//   4. alu_modules/pkg/pkg.alu (package entry point)
// For legacy string imports, uses existing behavior.
static std::string resolveModulePath(const std::string& moduleName, 
                                      bool isModulePath,
                                      const std::string& sourceDir,
                                      const std::string& stdPath) {
    if (!isModulePath) {
        // Legacy import: return as-is, driver will handle file search
        return moduleName;
    }
    
    // Convert :: to path separator and append .alu
    std::string relPath = moduleName;
    // Replace all "::" with "/"
    size_t pos = 0;
    while ((pos = relPath.find("::", pos)) != std::string::npos) {
        relPath.replace(pos, 2, "/");
    }
    std::string aluFile = relPath + ".alu";
    
    // 1. Try the standard library path (handles std::fs -> std/fs.alu)
    {
        std::string candidate = stdPath + "/" + aluFile;
        if (fileExists(candidate)) {
            return candidate;
        }
    }
    
    // 2. Try std path as parent (std::fs -> stdPath/../std/fs.alu is same as #1, 
    //    but also handle if stdPath itself IS the std dir)
    //    Actually just try relative to the source file directory
    {
        std::string candidate = sourceDir + "/" + aluFile;
        if (fileExists(candidate)) {
            return candidate;
        }
    }
    
    // 3. Try alu_modules/ directory
    {
        std::string candidate = "alu_modules/" + aluFile;
        if (fileExists(candidate)) {
            return candidate;
        }
    }
    
    // 4. Try alu_modules/pkg/pkg.alu pattern (last segment as both dir and file)
    {
        size_t lastSlash = relPath.find_last_of('/');
        std::string lastSegment = (lastSlash != std::string::npos) 
            ? relPath.substr(lastSlash + 1) : relPath;
        std::string candidate = "alu_modules/" + relPath + "/" + lastSegment + ".alu";
        if (fileExists(candidate)) {
            return candidate;
        }
    }
    
    // Nothing found, return the .alu path anyway — the driver will report the error
    return aluFile;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: alu_cxx <file.alu>" << std::endl;
        return 1;
    }

    std::string command = "build";
    std::string target = "";
    std::string targetTriple = "";
    bool createXcframework = false;
    std::string ndkPath = "";
    std::string jniPackage = "com.example.alu";
    std::string stdPath = ""; // Will be auto-detected if not set
    
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
        } else if (i == 1 && (arg == "install" || arg == "build")) {
            command = arg;
        } else {
            target = arg;
        }
    }
    
    // Auto-detect std library path: try relative to the compiler executable's directory
    if (stdPath.empty()) {
        std::string exeDir = getDirectory(argv[0]);
        if (fileExists(exeDir + "/std/io.alu")) {
            stdPath = exeDir;
        } else if (fileExists(exeDir + "/../std/io.alu")) {
            stdPath = exeDir + "/..";
        } else {
            stdPath = "."; // fallback to current directory
        }
    }

    bool targetAndroid = (targetTriple == "aarch64-linux-android");
    bool targetWasm = (targetTriple == "wasm32-unknown-emscripten");
    bool targetVulkan = (targetTriple == "spirv64-unknown-unknown");
    bool targetMetal = (targetTriple == "air64-apple-macos");
    bool targetIos = (targetTriple == "aarch64-apple-ios");
    bool targetIosSim = (targetTriple == "x86_64-apple-ios-simulator" || targetTriple == "aarch64-apple-ios-simulator");

    // XCFramework logic
    if (createXcframework) {
        if (target.empty()) {
            std::cerr << "Usage: alu_cxx build <file.alu> --create-xcframework" << std::endl;
            return 1;
        }
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
        if (target.empty()) {
            std::cerr << "Usage: alu_cxx install <user/repo>" << std::endl;
            return 1;
        }
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

    std::string filename = target;
    if (filename.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
        return 1;
    }

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
    
    Parser parser(tokens);
    try {
        std::unique_ptr<ProgramNode> ast = parser.parse();
        
        // Resolve imports (supports both legacy and module-path styles)
        std::string sourceDir = getDirectory(filename);
        bool hasImports = true;
        std::unordered_set<std::string> imported_files;
        while (hasImports) {
            hasImports = false;
            for (auto it = ast->declarations.begin(); it != ast->declarations.end(); ++it) {
                if (auto importNode = dynamic_cast<ImportNode*>(it->get())) {
                    // Resolve the module path
                    std::string resolvedPath = resolveModulePath(
                        importNode->moduleName, importNode->isModulePath, sourceDir, stdPath);
                    
                    // Remove ImportNode from AST
                    ast->declarations.erase(it); 
                    
                    // Skip already-imported files (deduplication)
                    if (imported_files.find(resolvedPath) != imported_files.end()) {
                        hasImports = true;
                        break;
                    }
                    imported_files.insert(resolvedPath);
                    
                    // Open the resolved file
                    std::ifstream mf(resolvedPath);
                    if (!mf.is_open() && !importNode->isModulePath) {
                        // Legacy fallback: try alu_modules/ paths
                        std::string modFile = importNode->moduleName;
                        std::string modulePath = "alu_modules/" + modFile;
                        mf.open(modulePath);
                        if (!mf.is_open()) {
                            modulePath = "alu_modules/" + modFile + "/" + modFile + ".alu";
                            mf.open(modulePath);
                        }
                    }
                    
                    if (!mf.is_open()) {
                        std::string errMsg = "Cannot open imported module: " + importNode->moduleName;
                        if (importNode->isModulePath) {
                            errMsg += "\n  Searched: " + stdPath + "/" + resolvedPath;
                            errMsg += "\n  Searched: " + sourceDir + "/" + resolvedPath;
                            errMsg += "\n  Searched: alu_modules/" + resolvedPath;
                        }
                        throw std::runtime_error(errMsg);
                    }
                    
                    std::cout << "[ALU CXX] Importing module: " << importNode->moduleName 
                              << " -> " << resolvedPath << std::endl;
                    std::stringstream mb; mb << mf.rdbuf();
                    Lexer ml(mb.str());
                    Parser mp(ml.tokenize());
                    auto mAst = mp.parse();
                    
                    // Insert all declarations from mAst into ast
                    for (auto& decl : mAst->declarations) {
                        ast->declarations.push_back(std::move(decl));
                    }
                    hasImports = true;
                    break; // iterator invalidated
                }
            }
        }
        
        std::cout << "[ALU CXX] Abstract Syntax Tree generated successfully:" << std::endl;
        std::cout << "==================================================" << std::endl;
        ast->print();
        std::cout << "==================================================" << std::endl;
        
        SemanticAnalyzer semanticAnalyzer;
        semanticAnalyzer.analyze(ast.get());
        
        Z3Verifier z3Verifier;
        z3Verifier.verify(ast.get());
        
        std::cout << "[ALU CXX] Ready for LLVM IR Translation." << std::endl;
        
        std::string targetArchStr = "x86_64";
        if (targetWasm) targetArchStr = "wasm";
        if (targetVulkan) targetArchStr = "vulkan";
        if (targetMetal) targetArchStr = "metal";
        
        LLVMCodeGen codegen(targetArchStr);
        ast->codegen(codegen);
        
        std::string outFilename = filename + ".ll";
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
        std::string baseFilename = filename.substr(0, filename.find_last_of("."));
        std::string outBinFilename = baseFilename;
        
        if (targetAndroid) outBinFilename += ".so";
        else if (targetWasm) outBinFilename += ".js";
        else if (targetVulkan) outBinFilename += ".spv";
        else if (targetMetal) outBinFilename += ".metallib";
        else if (targetIos || targetIosSim) outBinFilename += "-" + targetTriple + ".a";
        else outBinFilename += ".exe";
        
        std::string compileCommand;
        if (targetWasm) {
            compileCommand = "emcc -O3 -s WASM=1 -s EXPORTED_FUNCTIONS=\"['_process_image', '_apply_filter']\" -o " + outBinFilename + " " + outFilename + " std/image_backend.cpp std/yara_backend.cpp";
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
            compileCommand = clangPath + " -shared -fPIC -o " + outBinFilename + " " + outFilename + " jni_bridge.cpp std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp -lcrypto -lssl";
        } else if (targetVulkan) {
            compileCommand = "llvm-spirv -o " + outBinFilename + " " + outFilename;
        } else if (targetMetal) {
            compileCommand = "xcrun -sdk macosx metal -c " + outFilename + " -o " + baseFilename + ".air && xcrun -sdk macosx metallib " + baseFilename + ".air -o " + outBinFilename;
        } else if (targetIos) {
            std::string objFile = baseFilename + "-arm64.o";
            compileCommand = "clang -x ir " + outFilename + " std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp std/packet_backend.cpp -c -O3 -arch arm64 -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -miphoneos-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
        } else if (targetIosSim) {
            std::string arch = (targetTriple == "aarch64-apple-ios-simulator") ? "arm64" : "x86_64";
            std::string objFile = baseFilename + "-" + arch + "-sim.o";
            compileCommand = "clang -x ir " + outFilename + " std/image_backend.cpp std/yara_backend.cpp std/net_backend.cpp std/net_crypto.cpp std/crypto_backend.cpp std/packet_backend.cpp -c -O3 -arch " + arch + " -isysroot $(xcrun --sdk iphonesimulator --show-sdk-path) -mios-simulator-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
        } else {
            compileCommand = "clang++ -O3 -o " + outBinFilename + " " + outFilename + " std/fs_backend.cpp std/net_backend.cpp std/crypto_backend.cpp std/image_backend.cpp -lws2_32";
        }
        
        int result = std::system(compileCommand.c_str());
        
        if (result == 0) {
            std::cout << "[ALU CXX] Compilation Successful! Binary built at: " << outBinFilename << std::endl;
        } else {
            std::cerr << "[ALU CXX] Linker Error: Clang failed with exit code " << result << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n[COMPILER ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
