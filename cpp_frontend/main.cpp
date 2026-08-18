#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
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
    bool use_static = false;
    
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
        } else if (arg == "--static") {
            use_static = true;
        } else if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3" || arg == "-Os" || arg == "-Oz") {
            opt_level = arg;
        } else if (arg == "-g" || arg == "--debug") {
            emit_debug_info = true;
        } else if (i == 1 && (arg == "install" || arg == "build" || arg == "bindgen" || arg == "pkg" || arg == "init" || arg == "run" || arg == "update" || arg == "list" || arg == "search" || arg == "publish" || arg == "clean")) {
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
        
        std::cout << "[ALU CXX] Generating Objective-C Bridge (alu_ios_bridge.m)..." << std::endl;
        std::ofstream iosBridge("alu_ios_bridge.m");
        iosBridge << "#import <UIKit/UIKit.h>\n";
        iosBridge << "extern \"C\" {\n";
        iosBridge << "    int alu_os_get_screen_width() {\n";
        iosBridge << "        return (int)[UIScreen mainScreen].bounds.size.width;\n";
        iosBridge << "    }\n";
        iosBridge << "    void alu_os_show_toast(const char* msg) {\n";
        iosBridge << "        NSString *nmsg = [NSString stringWithUTF8String:msg];\n";
        iosBridge << "        UIAlertController *alert = [UIAlertController alertControllerWithTitle:@\"Toast\" message:nmsg preferredStyle:UIAlertControllerStyleAlert];\n";
        iosBridge << "        [[UIApplication sharedApplication].keyWindow.rootViewController presentViewController:alert animated:YES completion:nil];\n";
        iosBridge << "        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{\n";
        iosBridge << "            [alert dismissViewControllerAnimated:YES completion:nil];\n";
        iosBridge << "        });\n";
        iosBridge << "    }\n";
        iosBridge << "}\n";
        iosBridge.close();
        
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

    if (command == "pkg" || command == "install" || command == "init" || command == "run" || 
        command == "update" || command == "list" || command == "search" || 
        command == "publish" || command == "clean" || 
        (command == "build" && inputFiles.empty())) {
        
        std::string exeDir = ModuleLinker::getDirectory(argv[0]);
        std::string pmExe = exeDir + "/alupm.exe";
        if (!fileExists(pmExe)) {
            pmExe = "alupm"; // fallback to PATH
        }
        
        std::string cmd = pmExe;
        int startArg = 1;
        if (command != "pkg") {
            cmd += " " + command;
            startArg = 2; // skip the command since we appended it
        } else {
            startArg = 2; // skip "pkg"
        }
        
        for (int i = startArg; i < argc; ++i) {
            cmd += " ";
            cmd += argv[i];
        }
        return std::system(cmd.c_str());
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
            
            if (!semanticAnalyzer.errors.empty()) {
                for (const auto& err : semanticAnalyzer.errors) {
                    std::cerr << "Semantic Error in " << err.file << ":" << err.line << ":" << err.col << ": " << err.message << std::endl;
                }
                return 1;
            }
            
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
            std::cout << "[ALU CXX] Generating JNI Bridge and NativeActivity for package: " << jniPackage << std::endl;
            std::ofstream jniFile("jni_bridge.cpp");
            jniFile << "#include <jni.h>\n";
            jniFile << "#include <string>\n";
            jniFile << "#include <vector>\n";
            jniFile << "#include <iostream>\n";
            jniFile << "#include <pthread.h>\n";
            jniFile << "#include <android/native_activity.h>\n\n";
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
            
            std::string mainRoutineName = "";
            for (const auto& decl : ast->declarations) {
                if (auto rNode = dynamic_cast<RoutineNode*>(decl.get())) {
                    if (rNode->isExported) {
                        if (rNode->name == "main") {
                            mainRoutineName = "main";
                        }
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
            if (!mainRoutineName.empty()) {
                jniFile << "extern \"C\" {\n";
                jniFile << "    static JavaVM* g_vm = nullptr;\n";
                jniFile << "    static jobject g_activity = nullptr;\n";
                jniFile << "    static void* alu_thread_func(void*) {\n";
                jniFile << "        " << mainRoutineName << "();\n";
                jniFile << "        return nullptr;\n";
                jniFile << "    }\n";
                jniFile << "    JNIEXPORT void JNICALL ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {\n";
                jniFile << "        g_vm = activity->vm;\n";
                jniFile << "        JNIEnv* env;\n";
                jniFile << "        activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);\n";
                jniFile << "        g_activity = env->NewGlobalRef(activity->clazz);\n";
                jniFile << "        pthread_t thread;\n";
                jniFile << "        pthread_create(&thread, nullptr, alu_thread_func, nullptr);\n";
                jniFile << "    }\n";
                jniFile << "    int alu_os_get_screen_width() {\n";
                jniFile << "        if (!g_vm || !g_activity) return 0;\n";
                jniFile << "        JNIEnv* env; g_vm->AttachCurrentThread(&env, nullptr);\n";
                jniFile << "        jclass activityClass = env->GetObjectClass(g_activity);\n";
                jniFile << "        jmethodID getResources = env->GetMethodID(activityClass, \"getResources\", \"()Landroid/content/res/Resources;\");\n";
                jniFile << "        jobject resources = env->CallObjectMethod(g_activity, getResources);\n";
                jniFile << "        jclass resourcesClass = env->GetObjectClass(resources);\n";
                jniFile << "        jmethodID getDisplayMetrics = env->GetMethodID(resourcesClass, \"getDisplayMetrics\", \"()Landroid/util/DisplayMetrics;\");\n";
                jniFile << "        jobject metrics = env->CallObjectMethod(resources, getDisplayMetrics);\n";
                jniFile << "        jclass metricsClass = env->GetObjectClass(metrics);\n";
                jniFile << "        jfieldID widthPixels = env->GetFieldID(metricsClass, \"widthPixels\", \"I\");\n";
                jniFile << "        int width = env->GetIntField(metrics, widthPixels);\n";
                jniFile << "        g_vm->DetachCurrentThread();\n";
                jniFile << "        return width;\n";
                jniFile << "    }\n";
                jniFile << "    void alu_os_show_toast(char* msg) {\n";
                // Note: Toast needs UI thread, but we'll leave it simple for the hook demonstration
                jniFile << "        if (!g_vm || !g_activity) return;\n";
                jniFile << "        JNIEnv* env; g_vm->AttachCurrentThread(&env, nullptr);\n";
                jniFile << "        jclass toastClass = env->FindClass(\"android/widget/Toast\");\n";
                jniFile << "        jmethodID makeText = env->GetStaticMethodID(toastClass, \"makeText\", \"(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;\");\n";
                jniFile << "        jmethodID show = env->GetMethodID(toastClass, \"show\", \"()V\");\n";
                jniFile << "        jstring jmsg = env->NewStringUTF(msg);\n";
                jniFile << "        jobject toast = env->CallStaticObjectMethod(toastClass, makeText, g_activity, jmsg, 0);\n";
                jniFile << "        env->CallVoidMethod(toast, show);\n";
                jniFile << "        env->DeleteLocalRef(jmsg);\n";
                jniFile << "        g_vm->DetachCurrentThread();\n";
                jniFile << "    }\n";
                jniFile << "}\n\n";
            }
            jniFile.close();
            std::cout << "[ALU CXX] JNI Bridge and NativeActivity generated at jni_bridge.cpp" << std::endl;
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
#if defined(_WIN32)
                    const char* localAppdata = std::getenv("LOCALAPPDATA");
                    if (localAppdata) {
                        resolvedNdk = std::string(localAppdata) + "\\Android\\Sdk\\ndk\\25.1.8937393"; // Default fallback
                    }
#elif defined(__APPLE__)
                    const char* home = std::getenv("HOME");
                    if (home) {
                        resolvedNdk = std::string(home) + "/Library/Android/sdk/ndk/25.1.8937393";
                    }
#else
                    const char* home = std::getenv("HOME");
                    if (home) {
                        resolvedNdk = std::string(home) + "/Android/Sdk/ndk/25.1.8937393";
                    }
#endif
                }
            }

#if defined(_WIN32)
            std::string ndkHost = "windows-x86_64";
            std::string sep = "\\";
#elif defined(__APPLE__)
            std::string ndkHost = "darwin-x86_64";
            std::string sep = "/";
#else
            std::string ndkHost = "linux-x86_64";
            std::string sep = "/";
#endif
            std::string clangPath = resolvedNdk + sep + "toolchains" + sep + "llvm" + sep + "prebuilt" + sep + ndkHost + sep + "bin" + sep + "aarch64-linux-android30-clang++";
#ifdef _WIN32
            clangPath += ".cmd";
#endif
            compileCommand = clangPath + " -shared -fPIC -o " + outBinFilename + " " + outFilename + " jni_bridge.cpp \"" + stdPath + "/std/string_backend.cpp\" \"" + stdPath + "/std/image_backend.cpp\" \"" + stdPath + "/std/yara_backend.cpp\" \"" + stdPath + "/std/net_backend.cpp\" \"" + stdPath + "/std/net_crypto.cpp\" \"" + stdPath + "/std/crypto_backend.cpp\" -landroid -llog";
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
            compileCommand = "clang -x ir " + outFilename + " \"" + stdPath + "/std/string_backend.cpp\" \"" + stdPath + "/std/image_backend.cpp\" \"" + stdPath + "/std/yara_backend.cpp\" \"" + stdPath + "/std/net_backend.cpp\" \"" + stdPath + "/std/net_crypto.cpp\" \"" + stdPath + "/std/crypto_backend.cpp\" \"" + stdPath + "/std/packet_backend.cpp\" -c -O3 -arch arm64 -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -miphoneos-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
#endif
        } else if (targetIosSim) {
            std::string arch = (targetTriple == "aarch64-apple-ios-simulator") ? "arm64" : "x86_64";
            std::string objFile = baseFilename + "-" + arch + "-sim.o";
#ifdef _WIN32
            compileCommand = "clang -x ir " + outFilename + " -c -O3 -target " + targetTriple + " -o " + objFile;
            outBinFilename = objFile;
#else
            compileCommand = "clang -x ir " + outFilename + " \"" + stdPath + "/std/string_backend.cpp\" \"" + stdPath + "/std/image_backend.cpp\" \"" + stdPath + "/std/yara_backend.cpp\" \"" + stdPath + "/std/net_backend.cpp\" \"" + stdPath + "/std/net_crypto.cpp\" \"" + stdPath + "/std/crypto_backend.cpp\" \"" + stdPath + "/std/packet_backend.cpp\" -c -O3 -arch " + arch + " -isysroot $(xcrun --sdk iphonesimulator --show-sdk-path) -mios-simulator-version-min=12.0 -o " + objFile + " && libtool -static -o " + outBinFilename + " " + objFile;
#endif
        } else {
            std::string opt_flag = emit_debug_info ? "-O0 -g" : opt_level;
            compileCommand = "clang++ " + opt_flag + " -o " + outBinFilename + " " + outFilename + " \"" + stdPath + "/std/fs_backend.cpp\" \"" + stdPath + "/std/net_backend.cpp\" \"" + stdPath + "/std/crypto_backend.cpp\" \"" + stdPath + "/std/string_backend.cpp\" \"" + stdPath + "/std/image_backend.cpp\" \"" + stdPath + "/std/thread_backend.cpp\" -lws2_32";
            if (use_static) {
                compileCommand += " -static -static-libgcc -static-libstdc++";
            }
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
            
            // --- POST-BUILD DEPENDENCY BUNDLING --- //
            if (!targetAndroid && !targetWasm && !targetVulkan && !targetMetal && !targetIos && !targetIosSim) {
                std::cout << "[ALU CXX] Tracing dynamic dependencies..." << std::endl;
                
                std::string exeDir = outBinFilename;
                size_t lastSlash = exeDir.find_last_of("/\\");
                std::string outDir = (lastSlash == std::string::npos) ? "." : exeDir.substr(0, lastSlash);

#ifdef _WIN32
                std::string traceCmd = "llvm-objdump -p " + outBinFilename + " > " + baseFilename + "_deps.txt 2>nul";
                if (std::system(traceCmd.c_str()) == 0) {
                    std::ifstream depsFile(baseFilename + "_deps.txt");
                    std::string line;
                    std::vector<std::string> dlls;
                    while (std::getline(depsFile, line)) {
                        if (line.find("DLL Name:") != std::string::npos) {
                            size_t pos = line.find("DLL Name:");
                            std::string dll = line.substr(pos + 9);
                            dll.erase(0, dll.find_first_not_of(" \t\r\n"));
                            dll.erase(dll.find_last_not_of(" \t\r\n") + 1);
                            
                            std::string lower_dll = dll;
                            std::transform(lower_dll.begin(), lower_dll.end(), lower_dll.begin(), ::tolower);
                            if (lower_dll.find("kernel32") == std::string::npos &&
                                lower_dll.find("user32") == std::string::npos &&
                                lower_dll.find("advapi32") == std::string::npos &&
                                lower_dll.find("msvcrt") == std::string::npos &&
                                lower_dll.find("ws2_32") == std::string::npos &&
                                lower_dll.find("bcrypt") == std::string::npos &&
                                lower_dll.find("api-ms-win") == std::string::npos &&
                                lower_dll.find("crypt32") == std::string::npos) {
                                dlls.push_back(dll);
                            }
                        }
                    }
                    depsFile.close();
                    
                    for (const auto& dll : dlls) {
                        std::string whereCmd = "where " + dll + " > " + baseFilename + "_where.txt 2>nul";
                        if (std::system(whereCmd.c_str()) == 0) {
                            std::ifstream whereFile(baseFilename + "_where.txt");
                            std::string dllPath;
                            if (std::getline(whereFile, dllPath)) {
                                dllPath.erase(0, dllPath.find_first_not_of(" \t\r\n"));
                                dllPath.erase(dllPath.find_last_not_of(" \t\r\n") + 1);
                                std::string copyCmd = "copy /Y \"" + dllPath + "\" \"" + outDir + "\" >nul 2>nul";
                                std::system(copyCmd.c_str());
                                std::cout << "[ALU CXX] Bundled dependency: " << dll << std::endl;
                            }
                            whereFile.close();
                        }
                        std::remove((baseFilename + "_where.txt").c_str());
                    }
                }
                std::remove((baseFilename + "_deps.txt").c_str());
#else
                std::string traceCmd = "ldd " + outBinFilename + " > " + baseFilename + "_deps.txt 2>/dev/null";
                if (std::system(traceCmd.c_str()) == 0) {
                    std::ifstream depsFile(baseFilename + "_deps.txt");
                    std::string line;
                    while (std::getline(depsFile, line)) {
                        if (line.find("=>") != std::string::npos) {
                            size_t start = line.find("=>") + 2;
                            size_t end = line.find(" (0x");
                            if (end != std::string::npos) {
                                std::string soPath = line.substr(start, end - start);
                                soPath.erase(0, soPath.find_first_not_of(" \t"));
                                soPath.erase(soPath.find_last_not_of(" \t") + 1);
                                
                                if (!soPath.empty() && 
                                    soPath.find("libc.so") == std::string::npos &&
                                    soPath.find("libm.so") == std::string::npos &&
                                    soPath.find("libdl.so") == std::string::npos &&
                                    soPath.find("libpthread.so") == std::string::npos &&
                                    soPath.find("ld-linux") == std::string::npos &&
                                    soPath.find("/lib/") == std::string::npos &&
                                    soPath.find("/usr/lib/") == std::string::npos) {
                                    
                                    std::string copyCmd = "cp \"" + soPath + "\" \"" + outDir + "/\"";
                                    std::system(copyCmd.c_str());
                                    
                                    size_t lastSlashLib = soPath.find_last_of("/");
                                    std::string libName = (lastSlashLib == std::string::npos) ? soPath : soPath.substr(lastSlashLib + 1);
                                    std::cout << "[ALU CXX] Bundled dependency: " << libName << std::endl;
                                }
                            }
                        }
                    }
                    depsFile.close();
                }
                std::remove((baseFilename + "_deps.txt").c_str());
#endif
            }
        } else {
            std::cerr << "[ALU CXX] Linker Error: Clang failed with exit code " << result << std::endl;
            return 1;
        }
        
    return 0;
}
