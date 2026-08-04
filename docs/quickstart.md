# 10-Minute Quickstart

Welcome to the Alu Image Processing Engine Quickstart.

## Introduction
Alu is a mathematically verified language that compiles directly to LLVM IR, avoiding garbage collection pauses, and providing blazingly fast execution.

## Getting Started

1. **Install Alu CLI**:
```bash
winget install alu-cli
```
2. **Compile your first script**:
```alu
routine main() -> int {
    print("Hello Alu");
    return 0;
}
```
3. **Execute**:
```bash
alu_cxx script.alu
./script.exe
```

## Using SDKs
Alu can be integrated into your enterprise stack natively using our Android `.AAR`, iOS `.XCFramework`, or WASM NPM module.
