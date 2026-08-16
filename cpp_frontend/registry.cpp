#include "registry.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// ─── Constructor ─────────────────────────────────────────────────────────────

Registry::Registry(const std::string& registryUrl) : registryUrl(registryUrl) {
    // Cache registry index at ~/.alupm/registry/
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
    localIndexPath = (fs::path(home) / ".alupm" / "registry").string();
}

// ─── Update ──────────────────────────────────────────────────────────────────

bool Registry::update() {
    fs::create_directories(fs::path(localIndexPath).parent_path());

    if (fs::exists(localIndexPath) && fs::exists(fs::path(localIndexPath) / ".git")) {
        // Pull latest
        std::cout << "[ALUPM] Updating registry index..." << std::endl;
        std::string cmd = "git -C \"" + localIndexPath + "\" pull --quiet 2>&1";
        return std::system(cmd.c_str()) == 0;
    } else {
        // Clone fresh
        std::cout << "[ALUPM] Cloning registry index from " << registryUrl << "..." << std::endl;
        std::string cmd = "git clone --depth 1 \"" + registryUrl + "\" \"" + localIndexPath + "\" 2>&1";
        int result = std::system(cmd.c_str());
        if (result != 0) {
            // Registry doesn't exist yet — create local stub
            std::cout << "[ALUPM] Registry not available. Using local index." << std::endl;
            fs::create_directories(localIndexPath);
            return true; // Not a fatal error
        }
        return true;
    }
}

// ─── Search ──────────────────────────────────────────────────────────────────

std::vector<RegistryEntry> Registry::search(const std::string& query) const {
    std::vector<RegistryEntry> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (!fs::exists(localIndexPath)) return results;

    // Walk through the index directory looking for .json files
    for (const auto& entry : fs::recursive_directory_iterator(localIndexPath)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        if (entry.path().filename().string()[0] == '.') continue; // skip hidden

        auto registryEntry = parseEntryFile(entry.path().string());
        if (!registryEntry) continue;

        // Match against name or description
        std::string lowerName = registryEntry->name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        std::string lowerDesc = registryEntry->description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDesc.find(lowerQuery) != std::string::npos) {
            results.push_back(*registryEntry);
        }
    }

    return results;
}

// ─── Lookup ──────────────────────────────────────────────────────────────────

std::optional<RegistryEntry> Registry::lookup(const std::string& packageName) const {
    if (!fs::exists(localIndexPath)) return std::nullopt;

    // Try direct path: <index>/<subdir>/<name>.json
    std::string subdir = getIndexSubdir(packageName);
    std::string entryPath = (fs::path(localIndexPath) / subdir / (packageName + ".json")).string();

    if (fs::exists(entryPath)) {
        return parseEntryFile(entryPath);
    }

    // Also try flat structure: <index>/<name>.json
    entryPath = (fs::path(localIndexPath) / (packageName + ".json")).string();
    if (fs::exists(entryPath)) {
        return parseEntryFile(entryPath);
    }

    return std::nullopt;
}

// ─── Get Package URL ─────────────────────────────────────────────────────────

std::optional<std::string> Registry::getPackageUrl(const std::string& packageName, const std::string& version) const {
    auto entry = lookup(packageName);
    if (!entry || entry->versions.empty()) return std::nullopt;

    if (version.empty()) {
        // Return latest version's URL
        return entry->versions.back().gitUrl;
    }

    // Find specific version
    for (const auto& v : entry->versions) {
        if (v.version == version) return v.gitUrl;
    }

    return std::nullopt;
}

// ─── List All ────────────────────────────────────────────────────────────────

std::vector<std::string> Registry::listAll() const {
    std::vector<std::string> names;

    if (!fs::exists(localIndexPath)) return names;

    for (const auto& entry : fs::recursive_directory_iterator(localIndexPath)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        if (entry.path().filename().string()[0] == '.') continue;

        names.push_back(entry.path().stem().string());
    }

    std::sort(names.begin(), names.end());
    return names;
}

// ─── Get Local Index Path ────────────────────────────────────────────────────

std::string Registry::getLocalIndexPath() const {
    return localIndexPath;
}

// ─── Parse Entry File ────────────────────────────────────────────────────────

std::optional<RegistryEntry> Registry::parseEntryFile(const std::string& filepath) const {
    std::ifstream file(filepath);
    if (!file.is_open()) return std::nullopt;

    try {
        nlohmann::json j;
        file >> j;

        RegistryEntry entry;
        entry.name = j.value("name", fs::path(filepath).stem().string());
        entry.description = j.value("description", "");
        entry.license = j.value("license", "");

        if (j.contains("versions") && j["versions"].is_array()) {
            for (const auto& vj : j["versions"]) {
                RegistryEntry::VersionInfo vi;
                vi.version = vj.value("version", "0.0.0");
                vi.gitUrl = vj.value("git", "");
                vi.checksum = vj.value("checksum", "");
                vi.published = vj.value("published", "");
                entry.versions.push_back(vi);
            }
        }

        return entry;
    } catch (...) {
        return std::nullopt;
    }
}

// ─── Index Subdirectory ──────────────────────────────────────────────────────

std::string Registry::getIndexSubdir(const std::string& packageName) {
    if (packageName.size() < 2) return packageName;
    std::string sub = packageName.substr(0, 2);
    std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
    return sub;
}
