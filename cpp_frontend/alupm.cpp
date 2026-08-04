#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

void init_project(const std::string& name) {
    if (fs::exists("alu.toml")) {
        std::cerr << "Error: alu.toml already exists in this directory.\n";
        return;
    }
    
    std::ofstream toml("alu.toml");
    toml << "[package]\n";
    toml << "name = \"" << (name.empty() ? "alu_project" : name) << "\"\n";
    toml << "version = \"0.1.0\"\n\n";
    toml << "[dependencies]\n";
    toml << "# std = \"https://github.com/alu-lang/std\"\n";
    toml.close();
    
    fs::create_directory("src");
    std::ofstream main_alu("src/main.alu");
    main_alu << "import \"std/io.alu\";\n\n";
    main_alu << "routine main() -> int {\n";
    main_alu << "    print(\"Hello from Alu Package Manager!\");\n";
    main_alu << "    return 0;\n";
    main_alu << "}\n";
    main_alu.close();
    
    std::cout << "[ALUPM] Initialized new Alu project: " << (name.empty() ? "alu_project" : name) << "\n";
}

void install_package(const std::string& git_url) {
    fs::create_directory("alu_modules");
    
    // Simple package name extraction (e.g. https://github.com/foo/bar.git -> bar)
    std::string pkg_name = git_url;
    auto last_slash = git_url.find_last_of('/');
    if (last_slash != std::string::npos) {
        pkg_name = git_url.substr(last_slash + 1);
    }
    if (pkg_name.find(".git") != std::string::npos) {
        pkg_name = pkg_name.substr(0, pkg_name.length() - 4);
    }
    
    std::string target_dir = "alu_modules/" + pkg_name;
    if (fs::exists(target_dir)) {
        std::cout << "[ALUPM] Package " << pkg_name << " is already installed.\n";
        return;
    }
    
    std::cout << "[ALUPM] Fetching " << git_url << " into " << target_dir << "...\n";
    std::string cmd = "git clone " + git_url + " " + target_dir;
    int res = std::system(cmd.c_str());
    
    if (res == 0) {
        std::cout << "[ALUPM] Successfully installed " << pkg_name << ".\n";
        // Append to alu.toml
        std::ofstream toml("alu.toml", std::ios_base::app);
        toml << pkg_name << " = \"" << git_url << "\"\n";
        toml.close();
    } else {
        std::cerr << "[ALUPM] Error installing package.\n";
    }
}

void build_project() {
    if (!fs::exists("alu.toml")) {
        std::cerr << "[ALUPM] Error: No alu.toml found in current directory.\n";
        return;
    }
    
    if (!fs::exists("src/main.alu")) {
        std::cerr << "[ALUPM] Error: src/main.alu not found.\n";
        return;
    }
    
    std::cout << "[ALUPM] Compiling project...\n";
    // We assume `alu` (the rust frontend) and `alu_cxx` (the c++ backend) are in the PATH
    // And we tell `alu` to include `alu_modules` in its search paths via `-I alu_modules/`
    // (Assuming the Rust compiler supports it or we inject it into the environment).
    
    std::string cmd = "alu src/main.alu"; // In a real environment, we'd pass module paths
    std::cout << "[ALUPM] Running: " << cmd << "\n";
    int res = std::system(cmd.c_str());
    
    if (res == 0) {
        std::cout << "[ALUPM] Build successful! Invoking native backend...\n";
        std::string backend_cmd = "alu_cxx build src/main.alu";
        std::system(backend_cmd.c_str());
    } else {
        std::cerr << "[ALUPM] Build failed.\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: alupm <command> [args]\n";
        std::cerr << "Commands:\n";
        std::cerr << "  init [name]      Initialize a new Alu project\n";
        std::cerr << "  install <url>    Install a package from a Git URL\n";
        std::cerr << "  build            Compile the current project\n";
        return 1;
    }
    
    std::string cmd = argv[1];
    if (cmd == "init") {
        std::string name = (argc >= 3) ? argv[2] : "";
        init_project(name);
    } else if (cmd == "install") {
        if (argc < 3) {
            std::cerr << "Error: 'install' requires a Git URL.\n";
            return 1;
        }
        install_package(argv[2]);
    } else if (cmd == "build") {
        build_project();
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        return 1;
    }
    
    return 0;
}
