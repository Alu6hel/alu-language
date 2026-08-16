#include "package_fetcher.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ─── Constructor ─────────────────────────────────────────────────────────────

PackageFetcher::PackageFetcher(const std::string& projectDir)
    : projectDir(projectDir) {
    modulesDir = (fs::path(projectDir) / "alu_modules").string();
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool PackageFetcher::fetch(DependencySpec& dep) {
    // Ensure alu_modules/ exists
    fs::create_directories(modulesDir);

    std::string targetDir = getPackageDir(dep.name);

    // Already installed?
    if (fs::exists(targetDir)) {
        std::cout << "[ALUPM] Package '" << dep.name << "' is already installed." << std::endl;
        dep.resolvedCommit = getGitCommit(targetDir);
        dep.resolvedVersion = getGitVersion(targetDir);
        return true;
    }

    if (dep.isPath()) {
        // Local path dependency
        std::cout << "[ALUPM] Linking local package '" << dep.name << "' from " << dep.path << std::endl;
        return copyLocalPackage(dep.path, targetDir);
    } else if (dep.isGit()) {
        // Git dependency
        std::cout << "[ALUPM] Fetching '" << dep.name << "' from " << dep.gitUrl << "..." << std::endl;

        // If a specific version is requested, try to use it as a git tag
        std::string tag = "";
        if (dep.versionConstraint != "*") {
            auto ver = SemVer::parse(dep.versionConstraint);
            if (ver) {
                tag = "v" + ver->toString();
            }
        }

        if (gitClone(dep.gitUrl, targetDir, tag)) {
            dep.resolvedCommit = getGitCommit(targetDir);
            dep.resolvedVersion = getGitVersion(targetDir);
            std::cout << "[ALUPM] Successfully installed '" << dep.name << "'";
            if (!dep.resolvedVersion.empty()) std::cout << " (version " << dep.resolvedVersion << ")";
            std::cout << std::endl;
            return true;
        }
        return false;
    } else {
        // Registry package — try to fetch from registry
        return fetchFromRegistry(dep);
    }
}

bool PackageFetcher::fetchAll(std::vector<DependencySpec>& deps) {
    bool allOk = true;
    for (auto& dep : deps) {
        if (!fetch(dep)) {
            std::cerr << "[ALUPM] Failed to fetch package '" << dep.name << "'" << std::endl;
            allOk = false;
        }
    }
    return allOk;
}

bool PackageFetcher::remove(const std::string& packageName) {
    std::string targetDir = getPackageDir(packageName);
    if (!fs::exists(targetDir)) {
        std::cerr << "[ALUPM] Package '" << packageName << "' is not installed." << std::endl;
        return false;
    }

    std::error_code ec;
    fs::remove_all(targetDir, ec);
    if (ec) {
        std::cerr << "[ALUPM] Failed to remove '" << packageName << "': " << ec.message() << std::endl;
        return false;
    }

    std::cout << "[ALUPM] Removed package '" << packageName << "'." << std::endl;
    return true;
}

bool PackageFetcher::isInstalled(const std::string& packageName) const {
    return fs::exists(getPackageDir(packageName));
}

std::string PackageFetcher::getPackageDir(const std::string& packageName) const {
    return (fs::path(modulesDir) / packageName).string();
}

std::string PackageFetcher::getCacheDir() {
    std::string home;
#ifdef _WIN32
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) home = userProfile;
    else home = "C:\\Users\\Default";
#else
    const char* homeEnv = std::getenv("HOME");
    if (homeEnv) home = homeEnv;
    else home = "/tmp";
#endif
    std::string cacheDir = (fs::path(home) / ".alupm" / "cache").string();
    fs::create_directories(cacheDir);
    return cacheDir;
}

// ─── Git Operations ──────────────────────────────────────────────────────────

bool PackageFetcher::gitClone(const std::string& url, const std::string& targetDir, const std::string& tag) {
    std::string cmd;
    if (!tag.empty()) {
        // Try cloning a specific tag first
        cmd = "git clone --depth 1 --branch " + tag + " \"" + url + "\" \"" + targetDir + "\" 2>&1";
        int result = std::system(cmd.c_str());
        if (result == 0) return true;
        // If tag clone failed, fall through to clone default branch
        std::cout << "[ALUPM] Tag '" << tag << "' not found, cloning latest..." << std::endl;
    }

    cmd = "git clone --depth 1 \"" + url + "\" \"" + targetDir + "\" 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::string PackageFetcher::getGitVersion(const std::string& repoDir) {
    // Try to get the latest git tag
    std::string cmd = "git -C \"" + repoDir + "\" describe --tags --abbrev=0 2>/dev/null";
#ifdef _WIN32
    cmd = "git -C \"" + repoDir + "\" describe --tags --abbrev=0 2>NUL";
#endif

    // Use a temp file to capture output
    std::string tempFile = (fs::path(repoDir) / ".alupm_version_tmp").string();
    cmd += " > \"" + tempFile + "\"";
    int result = std::system(cmd.c_str());

    if (result == 0 && fs::exists(tempFile)) {
        std::ifstream f(tempFile);
        std::string version;
        std::getline(f, version);
        f.close();
        fs::remove(tempFile);
        // Strip leading 'v'
        if (!version.empty() && (version[0] == 'v' || version[0] == 'V')) {
            version = version.substr(1);
        }
        // Trim whitespace
        while (!version.empty() && (version.back() == '\n' || version.back() == '\r' || version.back() == ' ')) {
            version.pop_back();
        }
        return version;
    }

    fs::remove(tempFile);
    return "";
}

std::string PackageFetcher::getGitCommit(const std::string& repoDir) {
    std::string tempFile = (fs::path(repoDir) / ".alupm_commit_tmp").string();
    std::string cmd = "git -C \"" + repoDir + "\" rev-parse HEAD > \"" + tempFile + "\"";
#ifdef _WIN32
    cmd += " 2>NUL";
#else
    cmd += " 2>/dev/null";
#endif

    int result = std::system(cmd.c_str());
    if (result == 0 && fs::exists(tempFile)) {
        std::ifstream f(tempFile);
        std::string commit;
        std::getline(f, commit);
        f.close();
        fs::remove(tempFile);
        while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r' || commit.back() == ' ')) {
            commit.pop_back();
        }
        return commit;
    }

    fs::remove(tempFile);
    return "";
}

// ─── Local Path ──────────────────────────────────────────────────────────────

bool PackageFetcher::copyLocalPackage(const std::string& sourcePath, const std::string& targetDir) {
    std::string absSource = sourcePath;
    if (!fs::path(sourcePath).is_absolute()) {
        absSource = (fs::path(projectDir) / sourcePath).string();
    }

    if (!fs::exists(absSource)) {
        std::cerr << "[ALUPM] Local path does not exist: " << absSource << std::endl;
        return false;
    }

    std::error_code ec;
    fs::copy(absSource, targetDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[ALUPM] Failed to copy local package: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

// ─── Registry ────────────────────────────────────────────────────────────────

bool PackageFetcher::fetchFromRegistry(DependencySpec& dep) {
    // Default registry: try to clone from https://github.com/alu-lang/<name>
    std::string registryUrl = "https://github.com/alu-lang/" + dep.name;
    std::cout << "[ALUPM] Searching registry for '" << dep.name << "'..." << std::endl;
    std::cout << "[ALUPM] Trying " << registryUrl << std::endl;

    dep.gitUrl = registryUrl;
    std::string targetDir = getPackageDir(dep.name);

    if (gitClone(registryUrl, targetDir)) {
        dep.resolvedCommit = getGitCommit(targetDir);
        dep.resolvedVersion = getGitVersion(targetDir);
        std::cout << "[ALUPM] Successfully installed '" << dep.name << "' from registry." << std::endl;
        return true;
    }

    // Also try alu-lang/<name>.git
    std::cerr << "[ALUPM] Package '" << dep.name << "' not found in registry." << std::endl;
    std::cerr << "[ALUPM] Hint: Install from Git URL with: alupm install --git <url>" << std::endl;
    return false;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string PackageFetcher::extractPackageName(const std::string& url) {
    std::string name = url;
    auto lastSlash = url.find_last_of('/');
    if (lastSlash != std::string::npos) {
        name = url.substr(lastSlash + 1);
    }
    if (name.size() > 4 && name.substr(name.size() - 4) == ".git") {
        name = name.substr(0, name.size() - 4);
    }
    return name;
}
