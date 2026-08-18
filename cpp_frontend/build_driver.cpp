#include "build_driver.h"
#include "package_fetcher.h"
#include <iostream>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

// ─── Constructor ─────────────────────────────────────────────────────────────

BuildDriver::BuildDriver(const std::string& projectDir)
    : projectDir(projectDir) {}

// ─── Find Compiler ──────────────────────────────────────────────────────────

std::string BuildDriver::findCompiler() const {
    // 1. Check same directory as this executable
    // 2. Check PATH
    // 3. Check well-known locations

    // Try relative to project dir first
    std::string candidates[] = {
        (fs::path(projectDir) / "alu.exe").string(),
        (fs::path(projectDir) / "alu").string(),
        (fs::path(projectDir) / "alu_cxx.exe").string(),
        (fs::path(projectDir) / "alu_cxx").string(),
        (fs::path(projectDir) / ".." / "alu.exe").string(),
        (fs::path(projectDir) / ".." / "alu").string(),
    };

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }

    // Fall back to PATH
#ifdef _WIN32
    // Check if alu_cxx.exe is on PATH by trying 'where'
    if (std::system("where alu.exe >NUL 2>NUL") == 0) return "alu.exe";
    if (std::system("where alu_cxx.exe >NUL 2>NUL") == 0) return "alu_cxx.exe";
#else
    if (std::system("which alu >/dev/null 2>&1") == 0) return "alu";
    if (std::system("which alu_cxx >/dev/null 2>&1") == 0) return "alu_cxx";
#endif

    // Last resort
    return "alu_cxx";
}

// ─── Collect Source Files ────────────────────────────────────────────────────

std::vector<std::string> BuildDriver::collectSourceFiles(const std::string& dir) const {
    std::vector<std::string> files;
    if (!fs::exists(dir)) return files;

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".alu") {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

// ─── Build Compiler Command ─────────────────────────────────────────────────

std::string BuildDriver::buildCompilerCommand(const ProjectManifest& manifest,
                                              const std::string& compilerPath,
                                              const std::string& targetTriple,
                                              bool debug) const {
    std::string cmd = compilerPath;

    // Add build command
    cmd += " build";

    // Entry file
    std::string entryFile = manifest.entry;
    if (entryFile.empty()) entryFile = "src/main.alu";
    cmd += " \"" + (fs::path(projectDir) / entryFile).string() + "\"";

    // Let compiler resolve standard library automatically using its default std-path

    // Target triple
    if (!targetTriple.empty()) {
        cmd += " --target=" + targetTriple;
    }

    // Debug mode
    if (debug) {
        cmd += " -g";
    }

    return cmd;
}

// ─── Build ───────────────────────────────────────────────────────────────────

int BuildDriver::build(const std::string& targetTriple, bool debug) {
    std::cout << "[ALUPM] Starting build..." << std::endl;

    // 1. Load manifest
    DependencyResolver resolver(projectDir);
    ProjectManifest manifest;
    try {
        manifest = resolver.loadManifest();
    } catch (const std::exception& e) {
        std::cerr << "[ALUPM] Error: " << e.what() << std::endl;
        std::cerr << "[ALUPM] Make sure you have a valid alu.toml in the current directory." << std::endl;
        std::cerr << "[ALUPM] Run 'alupm init' to create one." << std::endl;
        return 1;
    }

    std::cout << "[ALUPM] Project: " << manifest.name << " v" << manifest.version << std::endl;

    // 2. Resolve and fetch dependencies
    if (!manifest.dependencies.empty()) {
        std::cout << "[ALUPM] Resolving " << manifest.dependencies.size() << " dependencies..." << std::endl;
        auto resolved = resolver.resolve(manifest);

        PackageFetcher fetcher(projectDir);
        if (!fetcher.fetchAll(resolved)) {
            std::cerr << "[ALUPM] Dependency resolution failed." << std::endl;
            return 1;
        }

        // Generate/update lockfile
        auto lockfile = resolver.generateLockfile(resolved);
        lockfile.saveToFile((fs::path(projectDir) / "alu.lock").string());
        std::cout << "[ALUPM] Lockfile updated." << std::endl;
    }

    // 3. Find compiler
    std::string compiler = findCompiler();
    std::cout << "[ALUPM] Using compiler: " << compiler << std::endl;

    // 4. Build
    std::string cmd = buildCompilerCommand(manifest, compiler, targetTriple, debug);
    std::cout << "[ALUPM] Running: " << cmd << std::endl;

    int result = std::system(cmd.c_str());

    if (result == 0) {
        // Compute output path
        std::string entryFile = manifest.entry.empty() ? "src/main.alu" : manifest.entry;
        
        if (targetTriple.find("android") != std::string::npos) {
            std::cout << "[ALUPM] Generating Android APK..." << std::endl;
            std::string soPath = (fs::path(projectDir) / entryFile).replace_extension(".so").string();
            std::string safeName = manifest.name;
            std::replace(safeName.begin(), safeName.end(), '-', '_');
            std::string pkgName = "com.alu." + safeName;
            
            // 1. Find SDK
            std::string sdkPath;
            const char* envSdk = std::getenv("ANDROID_HOME");
            if (!envSdk) envSdk = std::getenv("ANDROID_SDK_ROOT");
            if (envSdk) {
                sdkPath = envSdk;
            } else {
#if defined(_WIN32)
                const char* localAppdata = std::getenv("LOCALAPPDATA");
                if (localAppdata) sdkPath = std::string(localAppdata) + "\\Android\\Sdk";
#elif defined(__APPLE__)
                const char* home = std::getenv("HOME");
                if (home) sdkPath = std::string(home) + "/Library/Android/sdk";
#else
                const char* home = std::getenv("HOME");
                if (home) sdkPath = std::string(home) + "/Android/Sdk";
#endif
            }
            
            // 2. Find latest build-tools and android.jar
            std::string buildToolsPath = "";
            if (fs::exists(sdkPath + "/build-tools")) {
                for (const auto& entry : fs::directory_iterator(sdkPath + "/build-tools")) {
                    if (entry.is_directory()) {
                        buildToolsPath = entry.path().string(); // Just grab the last one
                    }
                }
            }
            std::string androidJar = "";
            if (fs::exists(sdkPath + "/platforms")) {
                for (const auto& entry : fs::directory_iterator(sdkPath + "/platforms")) {
                    if (entry.is_directory() && fs::exists(entry.path() / "android.jar")) {
                        androidJar = (entry.path() / "android.jar").string();
                    }
                }
            }
            
            if (buildToolsPath.empty() || androidJar.empty()) {
                std::cerr << "[ALUPM] Error: Could not find Android SDK build-tools or android.jar at " << sdkPath << std::endl;
                return 1;
            }
            
            // Generate AndroidManifest.xml
            std::string manifestXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            manifestXml += "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"" + pkgName + "\" android:versionCode=\"1\" android:versionName=\"1.0\">\n";
            manifestXml += "    <application android:label=\"" + manifest.name + "\" android:hasCode=\"false\">\n";
            manifestXml += "        <activity android:name=\"android.app.NativeActivity\" android:label=\"" + manifest.name + "\" android:configChanges=\"orientation|keyboardHidden\" android:exported=\"true\">\n";
            std::string libName = fs::path(entryFile).filename().replace_extension("").string();
            manifestXml += "            <meta-data android:name=\"android.app.lib_name\" android:value=\"" + libName + "\" />\n";
            manifestXml += "            <intent-filter>\n";
            manifestXml += "                <action android:name=\"android.intent.action.MAIN\" />\n";
            manifestXml += "                <category android:name=\"android.intent.category.LAUNCHER\" />\n";
            manifestXml += "            </intent-filter>\n";
            manifestXml += "        </activity>\n";
            manifestXml += "    </application>\n";
            manifestXml += "</manifest>\n";
            
            std::string manifestPath = (fs::path(projectDir) / "AndroidManifest.xml").string();
            std::ofstream mOut(manifestPath);
            mOut << manifestXml;
            mOut.close();
            
            std::string unalignedApk = (fs::path(projectDir) / "app.unaligned.apk").string();
            std::string alignedApk = (fs::path(projectDir) / "app.aligned.apk").string();
            std::string finalApk = (fs::path(projectDir) / (manifest.name + ".apk")).string();
            
#ifdef _WIN32
            std::string sep = "\\";
            std::string aaptCmd = "\"\"" + buildToolsPath + sep + "aapt.exe\" package -f -M \"" + manifestPath + "\" -I \"" + androidJar + "\" -F \"" + unalignedApk + "\"\"";
#else
            std::string sep = "/";
            std::string aaptCmd = "\"" + buildToolsPath + sep + "aapt\" package -f -M \"" + manifestPath + "\" -I \"" + androidJar + "\" -F \"" + unalignedApk + "\"";
#endif
            std::cout << "[ALUPM] Running aapt..." << std::endl;
            if (std::system(aaptCmd.c_str()) != 0) return 1;
            
            // Add lib
            std::string libDir = (fs::path(projectDir) / "lib" / "arm64-v8a").string();
            fs::create_directories(libDir);
            std::string targetSo = (fs::path(libDir) / ("lib" + libName + ".so")).string();
            if (fs::exists(targetSo)) fs::remove(targetSo);
            fs::copy_file(soPath, targetSo);
            
            std::string cwdBak = fs::current_path().string();
            fs::current_path(projectDir);
#ifdef _WIN32
            std::string addCmd = "\"\"" + buildToolsPath + sep + "aapt.exe\" add \"" + unalignedApk + "\" lib/arm64-v8a/lib" + libName + ".so\"";
#else
            std::string addCmd = "\"" + buildToolsPath + sep + "aapt\" add \"" + unalignedApk + "\" lib/arm64-v8a/lib" + libName + ".so";
#endif
            std::cout << "[ALUPM] Adding native library..." << std::endl;
            if (std::system(addCmd.c_str()) != 0) { fs::current_path(cwdBak); return 1; }
            fs::current_path(cwdBak);
            
            // Zipalign
#ifdef _WIN32
            std::string zipalignCmd = "\"\"" + buildToolsPath + sep + "zipalign.exe\" -f -p 4 \"" + unalignedApk + "\" \"" + alignedApk + "\"\"";
#else
            std::string zipalignCmd = "\"" + buildToolsPath + sep + "zipalign\" -f -p 4 \"" + unalignedApk + "\" \"" + alignedApk + "\"";
#endif
            std::cout << "[ALUPM] Running zipalign..." << std::endl;
            if (std::system(zipalignCmd.c_str()) != 0) return 1;
            
            // Sign
            std::string keystore = (fs::path(projectDir) / "debug.keystore").string();
            if (!fs::exists(keystore)) {
                std::cout << "[ALUPM] Generating debug keystore..." << std::endl;
                std::string keygenCmd = "keytool -genkeypair -keystore \"" + keystore + "\" -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname \"CN=Android Debug,O=Android,C=US\"";
                std::system(keygenCmd.c_str());
            }
            
#ifdef _WIN32
            std::string signCmd = "\"\"" + buildToolsPath + sep + "apksigner.bat\" sign --ks \"" + keystore + "\" --ks-pass pass:android \"" + alignedApk + "\"\"";
#else
            std::string signCmd = "\"" + buildToolsPath + sep + "apksigner\" sign --ks \"" + keystore + "\" --ks-pass pass:android \"" + alignedApk + "\"";
#endif
            std::cout << "[ALUPM] Running apksigner..." << std::endl;
            if (std::system(signCmd.c_str()) != 0) return 1;
            
            if (fs::exists(finalApk)) fs::remove(finalApk);
            fs::rename(alignedApk, finalApk);
            fs::remove(unalignedApk);
            fs::remove(manifestPath);
            fs::remove_all(fs::path(projectDir) / "lib");
            
            outputPath = finalApk;
            std::cout << "[ALUPM] Build successful! APK: " << finalApk << std::endl;
        } else {
            // The compiler generates the output adjacent to the source file
#ifdef _WIN32
            outputPath = (fs::path(projectDir) / entryFile).replace_extension(".exe").string();
#else
            outputPath = (fs::path(projectDir) / entryFile).replace_extension("").string();
#endif
            std::cout << "[ALUPM] Build successful!" << std::endl;
        }
    } else {
        std::cerr << "[ALUPM] Build failed with exit code " << result << std::endl;
    }

    return result;
}

// ─── Run ─────────────────────────────────────────────────────────────────────

int BuildDriver::run(const std::vector<std::string>& args, const std::string& targetTriple, bool debug) {
    int buildResult = build(targetTriple, debug);
    if (buildResult != 0) return buildResult;

    if (outputPath.empty() || !fs::exists(outputPath)) {
        std::cerr << "[ALUPM] Cannot find output binary." << std::endl;
        return 1;
    }

    std::cout << "[ALUPM] Running " << outputPath << "..." << std::endl;
    std::cout << "──────────────────────────────────────────────────" << std::endl;

    std::string cmd = outputPath;
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }

    return std::system(cmd.c_str());
}

// ─── Clean ───────────────────────────────────────────────────────────────────

void BuildDriver::clean() {
    std::cout << "[ALUPM] Cleaning build artifacts..." << std::endl;

    // Remove .ll files and executables generated from src/
    for (const auto& entry : fs::directory_iterator(projectDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".ll" || ext == ".o" || ext == ".obj") {
                fs::remove(entry.path());
                std::cout << "[ALUPM] Removed: " << entry.path().filename().string() << std::endl;
            }
        }
    }

    // Also clean src/ directory artifacts
    std::string srcDir = (fs::path(projectDir) / "src").string();
    if (fs::exists(srcDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".ll" || ext == ".o" || ext == ".obj") {
                    fs::remove(entry.path());
                }
            }
        }
    }

    std::cout << "[ALUPM] Clean complete." << std::endl;
}

// ─── Get Output Path ────────────────────────────────────────────────────────

std::string BuildDriver::getOutputPath() const {
    return outputPath;
}
