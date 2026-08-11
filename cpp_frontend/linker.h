#pragma once
#include "ast.h"
#include <string>
#include <unordered_set>
#include <memory>
#include <vector>

class ModuleLinker {
public:
    ModuleLinker(const std::string& stdPath);
    
    // Resolves all imports, parses the imported files, and appends their declarations
    // to the main AST in a single linear pass.
    void link(ProgramNode* mainAst, const std::string& sourceDir, const std::string& mainFile);

    // Helper to get directory from a file path
    static std::string getDirectory(const std::string& path);

private:
    std::string stdPath;
    std::unordered_set<std::string> imported_files;
    
    // Recursive function to resolve imports in a given AST
    void resolveImports(ProgramNode* ast, const std::string& currentDir, std::vector<std::unique_ptr<ASTNode>>& out_decls);
    
    // Resolves a module path to an absolute or relative filesystem path
    std::string resolveModulePath(const std::string& moduleName, bool isModulePath, const std::string& currentDir);
};
