#pragma once
#include "toml_parser.h"
#include "semver.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <filesystem>

// Describes a single dependency specification from alu.toml
struct DependencySpec {
    std::string name;
    std::string versionConstraint;   // e.g., "^1.2.3"
    std::string gitUrl;              // e.g., "https://github.com/alu-lang/crypto_utils"
    std::string path;                // local path dependency
    std::string resolvedVersion;     // filled in after resolution
    std::string resolvedCommit;      // git commit hash
    std::string checksum;            // sha256 checksum

    bool isGit() const { return !gitUrl.empty(); }
    bool isPath() const { return !path.empty(); }
    bool isRegistry() const { return !isGit() && !isPath(); }
};

// A resolved package in the lockfile
struct ResolvedPackage {
    std::string name;
    std::string version;
    std::string source;     // git URL or path
    std::string commit;     // git commit hash
    std::string checksum;   // integrity hash
};

// The lockfile representation
struct Lockfile {
    int version = 1;
    std::map<std::string, ResolvedPackage> packages;

    // Serialize to JSON
    nlohmann::json toJson() const;

    // Deserialize from JSON
    static Lockfile fromJson(const nlohmann::json& j);

    // Load from file
    static std::optional<Lockfile> loadFromFile(const std::string& filepath);

    // Save to file
    bool saveToFile(const std::string& filepath) const;
};

// Project manifest (parsed from alu.toml)
struct ProjectManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string license;
    std::string entry;                        // e.g., "src/main.alu"
    std::vector<std::string> authors;
    std::vector<DependencySpec> dependencies;

    // Parse from a TOML document
    static ProjectManifest fromToml(const TomlDocument& doc);

    // Serialize back to TOML document
    TomlDocument toToml() const;
};

class DependencyResolver {
public:
    DependencyResolver(const std::string& projectDir);

    // Load the project manifest
    ProjectManifest loadManifest();

    // Load existing lockfile (if any)
    std::optional<Lockfile> loadLockfile();

    // Resolve all dependencies and produce an install plan
    // Returns the flat list of packages to install (topologically sorted)
    std::vector<DependencySpec> resolve(const ProjectManifest& manifest);

    // Generate/update the lockfile from resolved dependencies
    Lockfile generateLockfile(const std::vector<DependencySpec>& resolved);

    // Add a dependency to the manifest
    void addDependency(ProjectManifest& manifest, const DependencySpec& dep);

    // Remove a dependency from the manifest
    void removeDependency(ProjectManifest& manifest, const std::string& name);

    // Save the manifest back to alu.toml
    void saveManifest(const ProjectManifest& manifest);

private:
    std::string projectDir;
    std::string manifestPath;
    std::string lockfilePath;
    std::string modulesDir;

    // Recursively resolve transitive dependencies
    void resolveTransitive(const DependencySpec& dep,
                           std::map<std::string, DependencySpec>& resolved,
                           std::vector<std::string>& order,
                           std::vector<std::string>& stack);

    // Parse a dependency's own alu.toml to find its transitive deps
    std::vector<DependencySpec> parseDependencyManifest(const std::string& depDir);
};
