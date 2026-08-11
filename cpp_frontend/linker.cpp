#include "linker.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

ModuleLinker::ModuleLinker(const std::string& stdPath) : stdPath(stdPath) {}

std::string ModuleLinker::getDirectory(const std::string& filepath) {
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash == std::string::npos) return ".";
    return filepath.substr(0, lastSlash);
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

std::string ModuleLinker::resolveModulePath(const std::string& moduleName, 
                                            bool isModulePath, 
                                            const std::string& currentDir) {
    if (!isModulePath) {
        // Legacy import
        return moduleName;
    }
    
    std::string relPath = moduleName;
    size_t pos = 0;
    while ((pos = relPath.find("::", pos)) != std::string::npos) {
        relPath.replace(pos, 2, "/");
    }
    std::string aluFile = relPath + ".alu";
    
    // 1. Try standard library
    {
        std::string candidate = stdPath + "/" + aluFile;
        if (fileExists(candidate)) return candidate;
    }
    
    // 2. Try relative to current source file directory
    {
        std::string candidate = currentDir + "/" + aluFile;
        if (fileExists(candidate)) return candidate;
    }
    
    // 3. Try alu_modules/ directory
    {
        std::string candidate = "alu_modules/" + aluFile;
        if (fileExists(candidate)) return candidate;
    }
    
    // 4. Try package entry point
    {
        size_t lastSlash = relPath.find_last_of('/');
        std::string lastSegment = (lastSlash != std::string::npos) 
            ? relPath.substr(lastSlash + 1) : relPath;
        std::string candidate = "alu_modules/" + relPath + "/" + lastSegment + ".alu";
        if (fileExists(candidate)) return candidate;
    }
    
    return aluFile;
}

void ModuleLinker::resolveImports(ProgramNode* ast, const std::string& currentDir, std::vector<std::unique_ptr<ASTNode>>& out_decls) {
    for (auto& decl : ast->declarations) {
        if (auto importNode = dynamic_cast<ImportNode*>(decl.get())) {
            std::string resolvedPath = resolveModulePath(importNode->moduleName, importNode->isModulePath, currentDir);
            
            // Skip if already imported
            if (imported_files.find(resolvedPath) == imported_files.end()) {
                imported_files.insert(resolvedPath);
                
                std::ifstream mf(resolvedPath);
                if (!mf.is_open() && !importNode->isModulePath) {
                    // Legacy fallback
                    std::string modFile = importNode->moduleName;
                    std::string modulePath = "alu_modules/" + modFile;
                    mf.open(modulePath);
                    if (!mf.is_open()) {
                        modulePath = "alu_modules/" + modFile + "/" + modFile + ".alu";
                        mf.open(modulePath);
                    }
                }
                
                if (!mf.is_open()) {
                    std::string errMsg = "Cannot open imported module: " + importNode->moduleName;
                    if (importNode->isModulePath) {
                        errMsg += "\n  Searched: " + stdPath + "/" + resolvedPath;
                        errMsg += "\n  Searched: " + currentDir + "/" + resolvedPath;
                        errMsg += "\n  Searched: alu_modules/" + resolvedPath;
                    }
                    throw std::runtime_error(errMsg);
                }
                
                std::cerr << "[ALU CXX Linker] Resolving module: " << importNode->moduleName 
                          << " -> " << resolvedPath << std::endl;
                
                std::stringstream mb; mb << mf.rdbuf();
                Lexer ml(mb.str());
                Parser mp(ml.tokenize(), resolvedPath);
                auto mAst = mp.parse();
                
                std::string modDir = getDirectory(resolvedPath);
                
                // Recursively resolve imports within this newly parsed AST
                std::vector<std::unique_ptr<ASTNode>> mod_decls;
                resolveImports(mAst.get(), modDir, mod_decls);
                
                if (!importNode->alias.empty()) {
                    auto nsNode = std::make_unique<NamespaceNode>(importNode->alias);
                    nsNode->declarations = std::move(mod_decls);
                    out_decls.push_back(std::move(nsNode));
                } else {
                    for (auto& d : mod_decls) {
                        out_decls.push_back(std::move(d));
                    }
                }
            }
        } else {
            // Not an import, just pass it through
            out_decls.push_back(std::move(decl));
        }
    }
}

void ModuleLinker::link(ProgramNode* mainAst, const std::string& sourceDir, const std::string& mainFile) {
    // Add the main file to imported_files to prevent circular dependency re-importing the entry point
    imported_files.insert(mainFile);
    
    std::vector<std::unique_ptr<ASTNode>> final_decls;
    resolveImports(mainAst, sourceDir, final_decls);
    mainAst->declarations = std::move(final_decls);
}
