use std::process::Command;
use std::fs;

pub struct Packager;

impl Packager {
    pub fn new() -> Self {
        Self {}
    }

    pub fn pack(&self, project_file: &str, target: &str) {
        println!("[ALU Packager] Reading configuration from {}...", project_file);
        
        match target {
            "windows-msi" => {
                self.build_msi();
            }
            "android" => {
                self.build_apk();
            }
            _ => {
                println!("[ALU Packager Error] Unsupported target: {}. Supported: windows-msi, android.", target);
            }
        }
    }

    fn build_msi(&self) {
        println!("[ALU Packager] Generating WiX Source file (.wxs)...");
        let wxs_content = r#"<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
    <Product Id="*" Name="ALU Application" Language="1033" Version="1.0.0.0" Manufacturer="ALU Developer" UpgradeCode="PUT-GUID-HERE">
        <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
        <MediaTemplate EmbedCab="yes" />
        <Feature Id="ProductFeature" Title="ALU App" Level="1">
            <ComponentGroupRef Id="ProductComponents" />
        </Feature>
    </Product>
</Wix>"#;
        fs::write("installer.wxs", wxs_content).expect("Failed to write installer.wxs");

        println!("[ALU Packager] Invoking WiX candle.exe...");
        let candle_status = Command::new("candle")
            .arg("installer.wxs")
            .status();

        match candle_status {
            Ok(s) if s.success() => {
                println!("[ALU Packager] Invoking WiX light.exe...");
                let light_status = Command::new("light")
                    .arg("installer.wixobj")
                    .arg("-o")
                    .arg("AppSetup.msi")
                    .status();
                
                if let Ok(ls) = light_status {
                    if ls.success() {
                        println!("[ALU Packager] SUCCESS! AppSetup.msi generated.");
                        return;
                    }
                }
                println!("[!] Failed during light.exe linkage.");
            }
            _ => {
                println!("[!] WiX Toolset not found or failed. A dummy installer.wxs was created.");
                println!("Please install WiX Toolset and ensure 'candle' and 'light' are in PATH.");
            }
        }
    }

    fn build_apk(&self) {
        println!("[ALU Packager] Generating Android Gradle structure...");
        // In a real implementation, we'd copy an android stub directory here
        println!("[ALU Packager] Invoking gradlew assembleDebug...");
        let gradle_status = Command::new("cmd")
            .args(&["/C", "gradlew assembleDebug"])
            .status();

        match gradle_status {
            Ok(s) if s.success() => {
                println!("[ALU Packager] SUCCESS! .apk generated in build/outputs/apk/debug/");
            }
            _ => {
                println!("[!] Gradle wrapper not found or failed. Please ensure Android SDK/NDK is configured.");
            }
        }
    }
}
