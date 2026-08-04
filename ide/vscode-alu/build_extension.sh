#!/bin/bash
set -e

echo "===================================================="
echo "[VS CODE EXTENSION] Building ALU Language Support..."
echo "===================================================="

# Install dependencies if node_modules doesn't exist
if [ ! -d "node_modules" ]; then
    echo "Installing NPM dependencies..."
    npm install
fi

# Package the extension using VSCE
echo "Packaging extension..."
npx vsce package

echo "[SUCCESS] Built .vsix extension successfully!"
