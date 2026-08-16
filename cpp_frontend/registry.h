#pragma once
#include "json.hpp"
#include <string>
#include <vector>
#include <optional>

// Central registry interaction for searching and publishing ALU packages.
// The registry is a Git-based index (similar to crates.io) where each package
// has a JSON metadata file with version history.

struct RegistryEntry {
    std::string name;
    std::string description;
    std::string license;

    struct VersionInfo {
        std::string version;
        std::string gitUrl;
        std::string checksum;
        std::string published;     // ISO date
    };

    std::vector<VersionInfo> versions;
};

class Registry {
public:
    // Default registry URL
    static constexpr const char* DEFAULT_REGISTRY = "https://github.com/alu-lang/registry";

    Registry(const std::string& registryUrl = DEFAULT_REGISTRY);

    // Update (clone/pull) the local registry index
    bool update();

    // Search for packages by name or keyword
    std::vector<RegistryEntry> search(const std::string& query) const;

    // Look up a specific package
    std::optional<RegistryEntry> lookup(const std::string& packageName) const;

    // Get the Git URL for a specific package version
    std::optional<std::string> getPackageUrl(const std::string& packageName, const std::string& version = "") const;

    // List all available packages
    std::vector<std::string> listAll() const;

    // Get the local cache path for the registry index
    std::string getLocalIndexPath() const;

private:
    std::string registryUrl;
    std::string localIndexPath;

    // Parse a registry entry from its JSON file
    std::optional<RegistryEntry> parseEntryFile(const std::string& filepath) const;

    // Get the index subdirectory for a package name (first 2 chars)
    static std::string getIndexSubdir(const std::string& packageName);
};
