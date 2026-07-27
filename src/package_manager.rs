use std::fs;
use std::path::Path;

pub struct PackageManager;

impl PackageManager {
    pub fn new() -> Self {
        Self {}
    }

    pub fn init_project(&self) {
        let manifest = r#"[project]
name = "alu_project"
version = "1.0.0"

[dependencies]
"#;
        fs::write("alu.toml", manifest).expect("Failed to create alu.toml");
        fs::create_dir_all("modules").expect("Failed to create modules directory");
        println!("[ALU Package Manager] Initialized new ALU project. Created `alu.toml` and `modules/`.");
    }

    pub fn fetch_dependencies(&self) {
        println!("[ALU Package Manager] Parsing alu.toml...");
        let toml_path = Path::new("alu.toml");
        if !toml_path.exists() {
            println!("[!] Error: alu.toml not found in current directory.");
            return;
        }

        let content = fs::read_to_string(toml_path).unwrap_or_default();
        let mut in_deps = false;

        let modules_dir = Path::new("modules");
        if !modules_dir.exists() {
            fs::create_dir_all(modules_dir).expect("Failed to create modules directory");
        }

        for line in content.lines() {
            let line = line.trim();
            if line.starts_with('[') && line.ends_with(']') {
                in_deps = line == "[dependencies]";
                continue;
            }

            if in_deps && !line.is_empty() {
                let parts: Vec<&str> = line.split('=').collect();
                if parts.len() == 2 {
                    let pkg_name = parts[0].trim();
                    let url = parts[1].trim().trim_matches('"');
                    
                    println!("[ALU Package Manager] Fetching package `{}` from {}...", pkg_name, url);
                    let target_dir = modules_dir.join(pkg_name);
                    
                    if target_dir.exists() {
                        println!(" -> Package `{}` already exists. Skipping.", pkg_name);
                        continue;
                    }

                    // Execute git clone
                    let status = std::process::Command::new("git")
                        .arg("clone")
                        .arg(url)
                        .arg(target_dir.to_str().unwrap())
                        .status();

                    match status {
                        Ok(s) if s.success() => {
                            println!(" -> Successfully cloned `{}`", pkg_name);
                        }
                        _ => {
                            println!(" -> [!] Failed to clone `{}`. Please verify the URL.", pkg_name);
                        }
                    }
                }
            }
        }
        println!("[ALU Package Manager] SUCCESS! Dependencies ready.");
    }

    pub fn install_package(&self, pkg_name: &str) {
        // Automatically adds a package to alu.toml
        println!("[ALU Package Manager] Adding `{}` to alu.toml...", pkg_name);
        if let Ok(mut content) = fs::read_to_string("alu.toml") {
            if !content.contains("[dependencies]") {
                content.push_str("\n[dependencies]\n");
            }
            content.push_str(&format!("{} = \"https://github.com/alu-ecosystem/{}.git\"\n", pkg_name, pkg_name));
            fs::write("alu.toml", content).expect("Failed to update alu.toml");
            println!("[ALU Package Manager] Updated alu.toml. Run `alu fetch` to download.");
        }
    }
}
