@echo off
echo ====================================================
echo [ALU PACKAGER] VS Code Extension Packager V1.0
echo ====================================================

echo [ALU PACKAGER] Packaging VSIX...
call npx @vscode/vsce package

echo [ALU PACKAGER] Publishing to Marketplace...
:: Note: Requires Personal Access Token to be set or prompted
:: call npx @vscode/vsce publish

echo [SUCCESS] Successfully packaged vscode-alu!
