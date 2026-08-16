// ──────────────────────────────────────────────────────────────────────────────
// ALUPM — The ALU Package Manager
// Copyright (c) 2026 David A. Jones / Alugandr Co. All rights reserved.
//
// A dependency management CLI for the ALU programming language.
// Supports: init, install, uninstall, build, run, update, list, search, publish, clean
// ──────────────────────────────────────────────────────────────────────────────

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include "toml_parser.h"
#include "semver.h"
#include "dependency_resolver.h"
#include "package_fetcher.h"
#include "build_driver.h"
#include "registry.h"

namespace fs = std::filesystem;

// ─── ANSI Color Helpers (Windows 10+ supports them) ─────────────────────────

#ifdef _WIN32
#include <windows.h>
static void enableAnsiColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    SetConsoleMode(hOut, dwMode);
}
#else
static void enableAnsiColors() {}
#endif

static const char* COLOR_RESET  = "\033[0m";
static const char* COLOR_GREEN  = "\033[32m";
static const char* COLOR_CYAN   = "\033[36m";
static const char* COLOR_YELLOW = "\033[33m";
static const char* COLOR_RED    = "\033[31m";
static const char* COLOR_BOLD   = "\033[1m";
static const char* COLOR_DIM    = "\033[2m";

static void printLogo() {
    std::cout << COLOR_CYAN << COLOR_BOLD;
    std::cout << R"(
     ___   __    __  __  ____  __  __ 
    /   | / /   / / / / / __ \/ /_/ /
   / /| |/ /   / / / / / /_/ / __  / 
  / ___ / /___/ /_/ / / ____/ / / /  
 /_/  |_\____/\____/ /_/   /_/ /_/   
                                      
)" << COLOR_RESET;
    std::cout << COLOR_DIM << " The ALU Package Manager v0.1.0" << COLOR_RESET << std::endl;
    std::cout << std::endl;
}

// ─── Version ─────────────────────────────────────────────────────────────────

static const char* ALUPM_VERSION = "0.1.0";

// ─── Help ────────────────────────────────────────────────────────────────────

static void printUsage() {
    printLogo();
    std::cout << COLOR_BOLD << "USAGE:" << COLOR_RESET << std::endl;
    std::cout << "    alupm <command> [args] [flags]" << std::endl;
    std::cout << std::endl;
    std::cout << COLOR_BOLD << "COMMANDS:" << COLOR_RESET << std::endl;
    std::cout << "    " << COLOR_GREEN << "init" << COLOR_RESET << " [name]           Create a new ALU project" << std::endl;
    std::cout << "    " << COLOR_GREEN << "install" << COLOR_RESET << " [pkg]         Install a package (Git URL, registry name, or from alu.toml)" << std::endl;
    std::cout << "    " << COLOR_GREEN << "uninstall" << COLOR_RESET << " <pkg>       Remove an installed package" << std::endl;
    std::cout << "    " << COLOR_GREEN << "build" << COLOR_RESET << "                 Compile the project" << std::endl;
    std::cout << "    " << COLOR_GREEN << "run" << COLOR_RESET << " [-- args]          Build and run the project" << std::endl;
    std::cout << "    " << COLOR_GREEN << "update" << COLOR_RESET << "                Update all dependencies to latest compatible versions" << std::endl;
    std::cout << "    " << COLOR_GREEN << "list" << COLOR_RESET << "                  List installed packages" << std::endl;
    std::cout << "    " << COLOR_GREEN << "search" << COLOR_RESET << " <query>        Search the package registry" << std::endl;
    std::cout << "    " << COLOR_GREEN << "publish" << COLOR_RESET << "               Publish this package to the registry" << std::endl;
    std::cout << "    " << COLOR_GREEN << "bindgen" << COLOR_RESET << " <header.h>    Generate Alu FFI bindings from a C header file" << std::endl;
    std::cout << "    " << COLOR_GREEN << "clean" << COLOR_RESET << "                 Remove build artifacts" << std::endl;
    std::cout << std::endl;
    std::cout << COLOR_BOLD << "FLAGS:" << COLOR_RESET << std::endl;
    std::cout << "    --version              Print version" << std::endl;
    std::cout << "    --help, -h             Print this help message" << std::endl;
    std::cout << "    --git <url>            Install from a specific Git URL" << std::endl;
    std::cout << "    --path <dir>           Install from a local path" << std::endl;
    std::cout << "    --target=<triple>      Cross-compile for a target (e.g., aarch64-linux-android)" << std::endl;
    std::cout << "    -g, --debug            Enable debug info" << std::endl;
    std::cout << std::endl;
}

// ─── Init Command ────────────────────────────────────────────────────────────

static int cmdInit(const std::string& name) {
    std::string projectName = name.empty() ? "alu_project" : name;

    if (fs::exists("alu.toml")) {
        std::cerr << COLOR_RED << "[ALUPM] Error: alu.toml already exists in this directory." << COLOR_RESET << std::endl;
        return 1;
    }

    // Create alu.toml
    TomlDocument doc;
    auto pkg = TomlValue::Table();
    pkg.tableVal["name"] = TomlValue::String(projectName);
    pkg.tableVal["version"] = TomlValue::String("0.1.0");
    pkg.tableVal["description"] = TomlValue::String("An ALU project");
    pkg.tableVal["license"] = TomlValue::String("MIT");
    pkg.tableVal["entry"] = TomlValue::String("src/main.alu");
    auto authors = TomlValue::Array();
    authors.arrayVal.push_back(TomlValue::String("Your Name"));
    pkg.tableVal["authors"] = authors;
    doc["package"] = pkg;

    auto deps = TomlValue::Table();
    doc["dependencies"] = deps;

    TomlParser::writeFile("alu.toml", doc);

    // Create src/main.alu
    fs::create_directory("src");
    std::ofstream mainFile("src/main.alu");
    mainFile << "// " << projectName << " — an ALU project" << std::endl;
    mainFile << "// Created by alupm" << std::endl;
    mainFile << std::endl;
    mainFile << "import std::io;" << std::endl;
    mainFile << std::endl;
    mainFile << "routine main() -> int {" << std::endl;
    mainFile << "    print(\"Hello from " << projectName << "!\");" << std::endl;
    mainFile << "    return 0;" << std::endl;
    mainFile << "}" << std::endl;
    mainFile.close();

    // Create alu_modules/
    fs::create_directory("alu_modules");

    // Create .gitignore
    if (!fs::exists(".gitignore")) {
        std::ofstream gitignore(".gitignore");
        gitignore << "# ALU build artifacts" << std::endl;
        gitignore << "*.ll" << std::endl;
        gitignore << "*.o" << std::endl;
        gitignore << "*.obj" << std::endl;
        gitignore << "*.exe" << std::endl;
        gitignore << "*.so" << std::endl;
        gitignore << "*.dylib" << std::endl;
        gitignore << std::endl;
        gitignore << "# Dependencies" << std::endl;
        gitignore << "alu_modules/" << std::endl;
        gitignore.close();
    }

    std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Initialized new ALU project: " 
              << COLOR_BOLD << projectName << COLOR_RESET << std::endl;
    std::cout << std::endl;
    std::cout << "  Created:" << std::endl;
    std::cout << "    " << COLOR_CYAN << "alu.toml" << COLOR_RESET << "         — project manifest" << std::endl;
    std::cout << "    " << COLOR_CYAN << "src/main.alu" << COLOR_RESET << "     — entry point" << std::endl;
    std::cout << "    " << COLOR_CYAN << "alu_modules/" << COLOR_RESET << "     — dependencies directory" << std::endl;
    std::cout << "    " << COLOR_CYAN << ".gitignore" << COLOR_RESET << "       — git ignore rules" << std::endl;
    std::cout << std::endl;
    std::cout << "  Next steps:" << std::endl;
    std::cout << "    " << COLOR_DIM << "alupm build" << COLOR_RESET << "       — compile the project" << std::endl;
    std::cout << "    " << COLOR_DIM << "alupm run" << COLOR_RESET << "         — build and run" << std::endl;
    std::cout << "    " << COLOR_DIM << "alupm install" << COLOR_RESET << "     — add dependencies" << std::endl;
    std::cout << std::endl;

    return 0;
}

// ─── Install Command ─────────────────────────────────────────────────────────

static int cmdInstall(const std::string& packageArg, const std::string& gitUrl, const std::string& pathDep) {
    std::string cwd = fs::current_path().string();
    DependencyResolver resolver(cwd);

    // If no argument, install all deps from alu.toml
    if (packageArg.empty() && gitUrl.empty() && pathDep.empty()) {
        if (!fs::exists("alu.toml")) {
            std::cerr << COLOR_RED << "[ALUPM] Error: No alu.toml found. Run 'alupm init' first." << COLOR_RESET << std::endl;
            return 1;
        }

        auto manifest = resolver.loadManifest();
        if (manifest.dependencies.empty()) {
            std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "No dependencies to install." << std::endl;
            return 0;
        }

        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Installing " << manifest.dependencies.size() << " dependencies..." << std::endl;
        auto resolved = resolver.resolve(manifest);
        PackageFetcher fetcher(cwd);
        if (!fetcher.fetchAll(resolved)) {
            return 1;
        }

        auto lockfile = resolver.generateLockfile(resolved);
        lockfile.saveToFile("alu.lock");
        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "All dependencies installed. Lockfile updated." << std::endl;
        return 0;
    }

    // Install a specific package
    DependencySpec dep;

    if (!gitUrl.empty()) {
        // Install from Git URL
        dep.name = PackageFetcher::extractPackageName(gitUrl);
        dep.gitUrl = gitUrl;
        dep.versionConstraint = "*";
    } else if (!pathDep.empty()) {
        // Install from local path
        dep.name = fs::path(pathDep).filename().string();
        dep.path = pathDep;
        dep.versionConstraint = "*";
    } else {
        // Parse package argument: could be "name", "name@version", or a URL
        std::string arg = packageArg;
        if (arg.find("://") != std::string::npos || arg.find("github.com") != std::string::npos) {
            dep.name = PackageFetcher::extractPackageName(arg);
            dep.gitUrl = arg;
        } else if (arg.find('/') != std::string::npos && arg.find("://") == std::string::npos) {
            // user/repo shorthand
            dep.name = arg.substr(arg.find('/') + 1);
            dep.gitUrl = "https://github.com/" + arg;
        } else {
            // Check for version specifier
            auto atPos = arg.find('@');
            if (atPos != std::string::npos) {
                dep.name = arg.substr(0, atPos);
                dep.versionConstraint = arg.substr(atPos + 1);
            } else {
                dep.name = arg;
                dep.versionConstraint = "*";
            }
        }
    }

    std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Installing '" << dep.name << "'..." << std::endl;

    PackageFetcher fetcher(cwd);
    if (!fetcher.fetch(dep)) {
        return 1;
    }

    // Update alu.toml with the new dependency
    if (fs::exists("alu.toml")) {
        auto manifest = resolver.loadManifest();
        resolver.addDependency(manifest, dep);
        resolver.saveManifest(manifest);
        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Updated alu.toml" << std::endl;

        // Update lockfile
        auto resolved = resolver.resolve(manifest);
        auto lockfile = resolver.generateLockfile(resolved);
        lockfile.saveToFile("alu.lock");
    }

    return 0;
}

// ─── Uninstall Command ───────────────────────────────────────────────────────

static int cmdUninstall(const std::string& packageName) {
    if (packageName.empty()) {
        std::cerr << COLOR_RED << "[ALUPM] Error: Package name required." << COLOR_RESET << std::endl;
        return 1;
    }

    std::string cwd = fs::current_path().string();
    PackageFetcher fetcher(cwd);

    if (!fetcher.remove(packageName)) {
        return 1;
    }

    // Update alu.toml
    if (fs::exists("alu.toml")) {
        DependencyResolver resolver(cwd);
        auto manifest = resolver.loadManifest();
        resolver.removeDependency(manifest, packageName);
        resolver.saveManifest(manifest);
        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Updated alu.toml" << std::endl;

        // Update lockfile
        auto resolved = resolver.resolve(manifest);
        auto lockfile = resolver.generateLockfile(resolved);
        lockfile.saveToFile("alu.lock");
    }

    return 0;
}

// ─── Build Command ───────────────────────────────────────────────────────────

static int cmdBuild(const std::string& targetTriple, bool debug) {
    std::string cwd = fs::current_path().string();
    BuildDriver driver(cwd);
    return driver.build(targetTriple, debug);
}

// ─── Run Command ─────────────────────────────────────────────────────────────

static int cmdRun(const std::vector<std::string>& args, const std::string& targetTriple, bool debug) {
    std::string cwd = fs::current_path().string();
    BuildDriver driver(cwd);
    return driver.run(args, targetTriple, debug);
}

// ─── Update Command ─────────────────────────────────────────────────────────

static int cmdUpdate() {
    std::string cwd = fs::current_path().string();

    if (!fs::exists("alu.toml")) {
        std::cerr << COLOR_RED << "[ALUPM] Error: No alu.toml found." << COLOR_RESET << std::endl;
        return 1;
    }

    DependencyResolver resolver(cwd);
    auto manifest = resolver.loadManifest();

    if (manifest.dependencies.empty()) {
        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "No dependencies to update." << std::endl;
        return 0;
    }

    std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Updating " << manifest.dependencies.size() << " dependencies..." << std::endl;

    // Remove all installed packages and re-fetch
    PackageFetcher fetcher(cwd);
    for (const auto& dep : manifest.dependencies) {
        if (fetcher.isInstalled(dep.name)) {
            fetcher.remove(dep.name);
        }
    }

    auto resolved = resolver.resolve(manifest);
    if (!fetcher.fetchAll(resolved)) {
        return 1;
    }

    auto lockfile = resolver.generateLockfile(resolved);
    lockfile.saveToFile("alu.lock");
    std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "All dependencies updated." << std::endl;
    return 0;
}

// ─── List Command ────────────────────────────────────────────────────────────

static int cmdList() {
    std::string cwd = fs::current_path().string();

    // Try reading lockfile first
    auto lockfile = Lockfile::loadFromFile((fs::path(cwd) / "alu.lock").string());
    if (lockfile && !lockfile->packages.empty()) {
        std::cout << COLOR_BOLD << "Installed packages:" << COLOR_RESET << std::endl;
        for (const auto& [name, pkg] : lockfile->packages) {
            std::cout << "  " << COLOR_CYAN << name << COLOR_RESET;
            if (!pkg.version.empty() && pkg.version != "*") {
                std::cout << " " << COLOR_GREEN << "v" << pkg.version << COLOR_RESET;
            }
            if (!pkg.source.empty()) {
                std::cout << " " << COLOR_DIM << "(" << pkg.source << ")" << COLOR_RESET;
            }
            std::cout << std::endl;
        }
        return 0;
    }

    // Fall back to scanning alu_modules/
    std::string modulesDir = (fs::path(cwd) / "alu_modules").string();
    if (!fs::exists(modulesDir)) {
        std::cout << COLOR_DIM << "No packages installed." << COLOR_RESET << std::endl;
        return 0;
    }

    std::cout << COLOR_BOLD << "Installed packages:" << COLOR_RESET << std::endl;
    int count = 0;
    for (const auto& entry : fs::directory_iterator(modulesDir)) {
        if (entry.is_directory()) {
            std::cout << "  " << COLOR_CYAN << entry.path().filename().string() << COLOR_RESET << std::endl;
            count++;
        }
    }

    if (count == 0) {
        std::cout << COLOR_DIM << "  No packages installed." << COLOR_RESET << std::endl;
    }

    return 0;
}

// ─── Search Command ──────────────────────────────────────────────────────────

static int cmdSearch(const std::string& query) {
    if (query.empty()) {
        std::cerr << COLOR_RED << "[ALUPM] Error: Search query required." << COLOR_RESET << std::endl;
        return 1;
    }

    Registry registry;
    registry.update();

    auto results = registry.search(query);
    if (results.empty()) {
        std::cout << COLOR_DIM << "No packages found matching '" << query << "'." << COLOR_RESET << std::endl;
        std::cout << std::endl;
        std::cout << "Hint: You can install directly from Git:" << std::endl;
        std::cout << "  alupm install --git https://github.com/user/repo" << std::endl;
        return 0;
    }

    std::cout << COLOR_BOLD << "Search results for '" << query << "':" << COLOR_RESET << std::endl;
    for (const auto& entry : results) {
        std::cout << "  " << COLOR_CYAN << entry.name << COLOR_RESET;
        if (!entry.versions.empty()) {
            std::cout << " " << COLOR_GREEN << "v" << entry.versions.back().version << COLOR_RESET;
        }
        if (!entry.description.empty()) {
            std::cout << " — " << entry.description;
        }
        std::cout << std::endl;
    }

    return 0;
}

// ─── Publish Command ─────────────────────────────────────────────────────────

static int cmdPublish() {
    if (!fs::exists("alu.toml")) {
        std::cerr << COLOR_RED << "[ALUPM] Error: No alu.toml found." << COLOR_RESET << std::endl;
        return 1;
    }

    std::string cwd = fs::current_path().string();
    DependencyResolver resolver(cwd);
    auto manifest = resolver.loadManifest();

    std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Preparing to publish '" << manifest.name 
              << "' v" << manifest.version << "..." << std::endl;

    // Create a package tarball
    std::string tarName = manifest.name + "-" + manifest.version + ".tar.gz";
    std::string cmd = "tar -czf \"" + tarName + "\" --exclude=alu_modules --exclude=.git --exclude=\"*.exe\" --exclude=\"*.o\" --exclude=\"*.ll\" .";
    int result = std::system(cmd.c_str());

    if (result == 0 && fs::exists(tarName)) {
        auto fileSize = fs::file_size(tarName);
        std::cout << COLOR_GREEN << "[ALUPM] " << COLOR_RESET << "Package created: " << tarName 
                  << " (" << fileSize << " bytes)" << std::endl;
        std::cout << std::endl;
        std::cout << "To publish to the registry, submit the following to the ALU registry:" << std::endl;
        std::cout << "  1. Create a Git repository for your package" << std::endl;
        std::cout << "  2. Tag the release: " << COLOR_DIM << "git tag v" << manifest.version << COLOR_RESET << std::endl;
        std::cout << "  3. Push: " << COLOR_DIM << "git push origin v" << manifest.version << COLOR_RESET << std::endl;
        std::cout << "  4. Submit your package URL to the ALU registry" << std::endl;
    } else {
        std::cerr << COLOR_RED << "[ALUPM] Failed to create package archive." << COLOR_RESET << std::endl;
        return 1;
    }

    return 0;
}

// ─── Clean Command ───────────────────────────────────────────────────────────

static int cmdClean() {
    std::string cwd = fs::current_path().string();
    BuildDriver driver(cwd);
    driver.clean();
    return 0;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    enableAnsiColors();

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    // Global flags
    if (command == "--version" || command == "-v") {
        std::cout << "alupm " << ALUPM_VERSION << std::endl;
        return 0;
    }
    if (command == "--help" || command == "-h" || command == "help") {
        printUsage();
        return 0;
    }

    // Parse remaining arguments
    std::vector<std::string> positionalArgs;
    std::string gitUrl;
    std::string pathDep;
    std::string targetTriple;
    bool debug = false;
    std::vector<std::string> runArgs;
    bool collectingRunArgs = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (collectingRunArgs) {
            runArgs.push_back(arg);
            continue;
        }

        if (arg == "--") {
            collectingRunArgs = true;
            continue;
        }

        if (arg == "--git" && i + 1 < argc) {
            gitUrl = argv[++i];
        } else if (arg == "--path" && i + 1 < argc) {
            pathDep = argv[++i];
        } else if (arg.find("--target=") == 0) {
            targetTriple = arg.substr(9);
        } else if (arg == "-g" || arg == "--debug") {
            debug = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            positionalArgs.push_back(arg);
        }
    }

    // Dispatch commands
    if (command == "init") {
        return cmdInit(positionalArgs.empty() ? "" : positionalArgs[0]);
    } else if (command == "install" || command == "add") {
        return cmdInstall(positionalArgs.empty() ? "" : positionalArgs[0], gitUrl, pathDep);
    } else if (command == "uninstall" || command == "remove") {
        return cmdUninstall(positionalArgs.empty() ? "" : positionalArgs[0]);
    } else if (command == "build") {
        return cmdBuild(targetTriple, debug);
    } else if (command == "run") {
        return cmdRun(runArgs, targetTriple, debug);
    } else if (command == "update" || command == "upgrade") {
        return cmdUpdate();
    } else if (command == "list" || command == "ls") {
        return cmdList();
    } else if (command == "search" || command == "find") {
        return cmdSearch(positionalArgs.empty() ? "" : positionalArgs[0]);
    } else if (command == "publish") {
        return cmdPublish();
    } else if (command == "bindgen") {
        std::string bindgenCmd = "alu_bindgen";
        for (int i = 2; i < argc; ++i) {
            bindgenCmd += " ";
            bindgenCmd += argv[i];
        }
        return std::system(bindgenCmd.c_str());
    } else if (command == "clean") {
        return cmdClean();
    } else {
        std::cerr << COLOR_RED << "[ALUPM] Unknown command: " << command << COLOR_RESET << std::endl;
        std::cerr << "Run 'alupm --help' for usage." << std::endl;
        return 1;
    }
}
