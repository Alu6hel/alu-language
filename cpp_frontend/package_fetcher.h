#pragma once
#include "dependency_resolver.h"
#include <string>
#include <vector>
#include <filesystem>

// Handles retrieving packages from Git repositories, local paths, and the central registry.

class PackageFetcher {
public:
    PackageFetcher(const std::string& projectDir);

    // Fetch a single package and install it to alu_modules/
    // Returns true on success
    bool fetch(DependencySpec& dep);

    // Fetch all dependencies
    bool fetchAll(std::vector<DependencySpec>& deps);

    // Remove a package from alu_modules/
    bool remove(const std::string& packageName);

    // Check if a package is already installed
    bool isInstalled(const std::string& packageName) const;

    // Get the install directory for a package
    std::string getPackageDir(const std::string& packageName) const;

    // Get the global cache directory (~/.alupm/cache/)
    static std::string getCacheDir();

    // Derive package name from a Git URL
    static std::string extractPackageName(const std::string& url);

private:
    std::string projectDir;
    std::string modulesDir;

    // Clone a Git repository
    bool gitClone(const std::string& url, const std::string& targetDir, const std::string& tag = "");

    // Get the latest git tag or HEAD commit
    std::string getGitVersion(const std::string& repoDir);

    // Get the HEAD commit hash
    std::string getGitCommit(const std::string& repoDir);

    // Copy a local path dependency
    bool copyLocalPackage(const std::string& sourcePath, const std::string& targetDir);

    // Fetch from registry (resolves name to git URL via registry index)
    bool fetchFromRegistry(DependencySpec& dep);
};
