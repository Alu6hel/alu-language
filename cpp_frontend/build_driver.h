#pragma once
#include "dependency_resolver.h"
#include <string>
#include <vector>

// Orchestrates the full build pipeline:
// 1. Reads alu.toml for project config
// 2. Ensures all dependencies are installed
// 3. Constructs and invokes the alu_cxx compiler command
// 4. Supports build + run mode

class BuildDriver {
public:
    BuildDriver(const std::string& projectDir);

    // Full build: resolve deps + compile
    int build(const std::string& targetTriple = "", bool debug = false);

    // Build and then execute the output binary
    int run(const std::vector<std::string>& args = {}, const std::string& targetTriple = "", bool debug = false);

    // Clean build artifacts
    void clean();

    // Get the output binary path
    std::string getOutputPath() const;

private:
    std::string projectDir;
    std::string outputPath;

    // Find the alu_cxx compiler binary
    std::string findCompiler() const;

    // Construct the compiler command line
    std::string buildCompilerCommand(const ProjectManifest& manifest,
                                     const std::string& compilerPath,
                                     const std::string& targetTriple,
                                     bool debug) const;

    // Collect all .alu source files from a dependency
    std::vector<std::string> collectSourceFiles(const std::string& dir) const;
};
