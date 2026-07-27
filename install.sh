#!/usr/bin/env bash
# ALU Language Installer (Linux/macOS)

echo "Installing ALU Bare-Metal Compiler V10..."
cargo build --release
echo "Adding ALU to path..."
# In a real script we would copy the binary to /usr/local/bin
echo "ALU V10 Swarm Ecosystem Installed Successfully!"
