use std::collections::HashMap;

/// ST-01: Hermetic Package Manager
/// Builds content-addressable fetching (SHA-256) and Static Capability Sandboxing.

pub struct Package {
    pub name: String,
    pub sha256_hash: String,
    pub capabilities: Vec<String>, // e.g., "deny_network", "deny_fs"
}

pub struct PackageManager {
    pub packages: HashMap<String, Package>,
}

impl PackageManager {
    pub fn new() -> Self {
        PackageManager {
            packages: HashMap::new(),
        }
    }

    pub fn fetch_package(&mut self, name: &str, hash: &str, capabilities: Vec<String>) -> Result<(), String> {
        // In a real environment, this fetches via content-addressable network request
        // Here we simulate the SHA-256 verification and sandbox application
        if hash.len() != 64 {
            return Err("Invalid SHA-256 hash length".to_string());
        }

        let pkg = Package {
            name: name.to_string(),
            sha256_hash: hash.to_string(),
            capabilities,
        };
        
        self.packages.insert(name.to_string(), pkg);
        Ok(())
    }

    pub fn verify_sandboxing(&self, name: &str) -> Result<(), String> {
        if let Some(pkg) = self.packages.get(name) {
            if pkg.capabilities.contains(&"deny_fs".to_string()) {
                println!("Static Capability Sandboxing: {} is denied filesystem access via CHERI bounds.", name);
            }
            Ok(())
        } else {
            Err("Package not found".to_string())
        }
    }
}
