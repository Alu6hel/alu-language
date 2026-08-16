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
        (fs::path(projectDir) / "alu_cxx.exe").string(),
        (fs::path(projectDir) / "alu_cxx").string(),
        (fs::path(projectDir) / ".." / "alu_cxx.exe").string(),
        (fs::path(projectDir) / ".." / "alu_cxx").string(),
        (fs::path(projectDir) / "cpp_frontend" / "alu_cxx.exe").string(),
        (fs::path(projectDir) / "cpp_frontend" / "alu.exe").string(),
    };

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }

    // Fall back to PATH
#ifdef _WIN32
    // Check if alu_cxx.exe is on PATH by trying 'where'
    if (std::system("where alu_cxx.exe >NUL 2>NUL") == 0) return "alu_cxx.exe";
    if (std::system("where alu.exe >NUL 2>NUL") == 0) return "alu.exe";
#else
    if (std::system("which alu_cxx >/dev/null 2>&1") == 0) return "alu_cxx";
    if (std::system("which alu >/dev/null 2>&1") == 0) return "alu";
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
    std::string cmd = "\"" + compilerPath + "\"";

    // Add build command
    cmd += " build";

    // Entry file
    std::string entryFile = manifest.entry;
    if (entryFile.empty()) entryFile = "src/main.alu";
    cmd += " \"" + (fs::path(projectDir) / entryFile).string() + "\"";

    // Set std path (modules directory may contain std libs)
    cmd += " --std-path \"" + projectDir + "\"";

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
        std::string baseName = fs::path(entryFile).stem().string();
#ifdef _WIN32
        outputPath = (fs::path(projectDir) / (baseName + ".exe")).string();
#else
        outputPath = (fs::path(projectDir) / baseName).string();
#endif
        std::cout << "[ALUPM] Build successful!" << std::endl;
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

    std::string cmd = "\"" + outputPath + "\"";
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
