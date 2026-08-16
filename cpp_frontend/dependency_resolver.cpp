#include "dependency_resolver.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace fs = std::filesystem;

// ─── Lockfile ────────────────────────────────────────────────────────────────

nlohmann::json Lockfile::toJson() const {
    nlohmann::json j;
    j["version"] = version;
    nlohmann::json pkgs = nlohmann::json::object();
    for (const auto& [name, pkg] : packages) {
        nlohmann::json p;
        p["version"] = pkg.version;
        p["source"] = pkg.source;
        if (!pkg.commit.empty()) p["commit"] = pkg.commit;
        if (!pkg.checksum.empty()) p["checksum"] = pkg.checksum;
        pkgs[name] = p;
    }
    j["packages"] = pkgs;
    return j;
}

Lockfile Lockfile::fromJson(const nlohmann::json& j) {
    Lockfile lf;
    if (j.contains("version")) lf.version = j["version"].get<int>();
    if (j.contains("packages") && j["packages"].is_object()) {
        for (auto& [name, val] : j["packages"].items()) {
            ResolvedPackage pkg;
            pkg.name = name;
            if (val.contains("version")) pkg.version = val["version"].get<std::string>();
            if (val.contains("source")) pkg.source = val["source"].get<std::string>();
            if (val.contains("commit")) pkg.commit = val["commit"].get<std::string>();
            if (val.contains("checksum")) pkg.checksum = val["checksum"].get<std::string>();
            lf.packages[name] = pkg;
        }
    }
    return lf;
}

std::optional<Lockfile> Lockfile::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return std::nullopt;
    try {
        nlohmann::json j;
        file >> j;
        return fromJson(j);
    } catch (...) {
        return std::nullopt;
    }
}

bool Lockfile::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << toJson().dump(2) << std::endl;
    return true;
}

// ─── ProjectManifest ─────────────────────────────────────────────────────────

ProjectManifest ProjectManifest::fromToml(const TomlDocument& doc) {
    ProjectManifest manifest;

    // [package] section
    auto pkgIt = doc.find("package");
    if (pkgIt != doc.end() && pkgIt->second.isTable()) {
        const auto& pkg = pkgIt->second;
        manifest.name = pkg.getString("name", "unnamed");
        manifest.version = pkg.getString("version", "0.1.0");
        manifest.description = pkg.getString("description", "");
        manifest.license = pkg.getString("license", "");
        manifest.entry = pkg.getString("entry", "src/main.alu");

        auto* authors = pkg.get("authors");
        if (authors && authors->isArray()) {
            for (const auto& a : authors->arrayVal) {
                if (a.isString()) manifest.authors.push_back(a.stringVal);
            }
        }
    }

    // Also check [project] section (legacy format used in existing alu.toml)
    auto projIt = doc.find("project");
    if (projIt != doc.end() && projIt->second.isTable()) {
        const auto& proj = projIt->second;
        if (manifest.name == "unnamed") manifest.name = proj.getString("name", "unnamed");
    }

    // [dependencies] section
    auto depsIt = doc.find("dependencies");
    if (depsIt != doc.end() && depsIt->second.isTable()) {
        for (const auto& [name, val] : depsIt->second.tableVal) {
            DependencySpec dep;
            dep.name = name;

            if (val.isString()) {
                // Simple string: could be a version, git URL, or file path
                const std::string& s = val.stringVal;
                if (s.find("://") != std::string::npos || s.find("github.com") != std::string::npos) {
                    dep.gitUrl = s;
                    dep.versionConstraint = "*";
                } else if (s.find("file:///") == 0 || s.find("./") == 0 || s.find("../") == 0 || s.find("/") == 0 || s.find("\\") == 0) {
                    // Path-based dependency
                    std::string path = s;
                    if (path.find("file:///") == 0) path = path.substr(8);
                    dep.path = path;
                    dep.versionConstraint = "*";
                } else {
                    dep.versionConstraint = s;
                }
            } else if (val.isTable()) {
                // Inline table: { version = "^1.0", git = "..." }
                auto* verVal = val.get("version");
                if (verVal && verVal->isString()) dep.versionConstraint = verVal->stringVal;
                else dep.versionConstraint = "*";

                auto* gitVal = val.get("git");
                if (gitVal && gitVal->isString()) dep.gitUrl = gitVal->stringVal;

                auto* pathVal = val.get("path");
                if (pathVal && pathVal->isString()) dep.path = pathVal->stringVal;
            }

            manifest.dependencies.push_back(dep);
        }
    }

    return manifest;
}

TomlDocument ProjectManifest::toToml() const {
    TomlDocument doc;

    // [package]
    auto pkg = TomlValue::Table();
    pkg.tableVal["name"] = TomlValue::String(name);
    pkg.tableVal["version"] = TomlValue::String(version);
    if (!description.empty()) pkg.tableVal["description"] = TomlValue::String(description);
    if (!license.empty()) pkg.tableVal["license"] = TomlValue::String(license);
    if (!entry.empty()) pkg.tableVal["entry"] = TomlValue::String(entry);
    if (!authors.empty()) {
        auto arr = TomlValue::Array();
        for (const auto& a : authors) arr.arrayVal.push_back(TomlValue::String(a));
        pkg.tableVal["authors"] = arr;
    }
    doc["package"] = pkg;

    // [dependencies]
    auto deps = TomlValue::Table();
    for (const auto& dep : dependencies) {
        if (dep.isGit()) {
            auto inline_tbl = TomlValue::InlineTable();
            inline_tbl.tableVal["version"] = TomlValue::String(dep.versionConstraint);
            inline_tbl.tableVal["git"] = TomlValue::String(dep.gitUrl);
            deps.tableVal[dep.name] = inline_tbl;
        } else if (dep.isPath()) {
            auto inline_tbl = TomlValue::InlineTable();
            inline_tbl.tableVal["path"] = TomlValue::String(dep.path);
            deps.tableVal[dep.name] = inline_tbl;
        } else {
            deps.tableVal[dep.name] = TomlValue::String(dep.versionConstraint);
        }
    }
    doc["dependencies"] = deps;

    return doc;
}

// ─── DependencyResolver ─────────────────────────────────────────────────────

DependencyResolver::DependencyResolver(const std::string& projectDir)
    : projectDir(projectDir) {
    manifestPath = (fs::path(projectDir) / "alu.toml").string();
    lockfilePath = (fs::path(projectDir) / "alu.lock").string();
    modulesDir = (fs::path(projectDir) / "alu_modules").string();
}

ProjectManifest DependencyResolver::loadManifest() {
    auto doc = TomlParser::parseFile(manifestPath);
    return ProjectManifest::fromToml(doc);
}

std::optional<Lockfile> DependencyResolver::loadLockfile() {
    return Lockfile::loadFromFile(lockfilePath);
}

std::vector<DependencySpec> DependencyResolver::parseDependencyManifest(const std::string& depDir) {
    std::string depManifest = (fs::path(depDir) / "alu.toml").string();
    if (!fs::exists(depManifest)) return {};

    try {
        auto doc = TomlParser::parseFile(depManifest);
        auto manifest = ProjectManifest::fromToml(doc);
        return manifest.dependencies;
    } catch (...) {
        return {};
    }
}

void DependencyResolver::resolveTransitive(const DependencySpec& dep,
                                           std::map<std::string, DependencySpec>& resolved,
                                           std::vector<std::string>& order,
                                           std::vector<std::string>& stack) {
    // Circular dependency check
    for (const auto& s : stack) {
        if (s == dep.name) {
            std::string cycle;
            for (const auto& c : stack) cycle += c + " -> ";
            cycle += dep.name;
            std::cerr << "[ALUPM] Warning: Circular dependency detected: " << cycle << std::endl;
            return;
        }
    }

    // Already resolved
    if (resolved.find(dep.name) != resolved.end()) {
        // Check version compatibility
        auto& existing = resolved[dep.name];
        if (dep.versionConstraint != "*" && existing.versionConstraint != dep.versionConstraint) {
            std::cerr << "[ALUPM] Warning: Version conflict for " << dep.name
                      << ": " << existing.versionConstraint << " vs " << dep.versionConstraint << std::endl;
        }
        return;
    }

    stack.push_back(dep.name);
    resolved[dep.name] = dep;

    // If the package is installed, check for its own dependencies
    std::string depDir = (fs::path(modulesDir) / dep.name).string();
    if (fs::exists(depDir)) {
        auto transitiveDeps = parseDependencyManifest(depDir);
        for (const auto& tdep : transitiveDeps) {
            resolveTransitive(tdep, resolved, order, stack);
        }
    }

    order.push_back(dep.name);
    stack.pop_back();
}

std::vector<DependencySpec> DependencyResolver::resolve(const ProjectManifest& manifest) {
    std::map<std::string, DependencySpec> resolved;
    std::vector<std::string> order;
    std::vector<std::string> stack;

    for (const auto& dep : manifest.dependencies) {
        resolveTransitive(dep, resolved, order, stack);
    }

    // Build result in topological order
    std::vector<DependencySpec> result;
    for (const auto& name : order) {
        result.push_back(resolved[name]);
    }
    return result;
}

Lockfile DependencyResolver::generateLockfile(const std::vector<DependencySpec>& deps) {
    Lockfile lf;
    lf.version = 1;

    for (const auto& dep : deps) {
        ResolvedPackage pkg;
        pkg.name = dep.name;
        pkg.version = dep.resolvedVersion.empty() ? dep.versionConstraint : dep.resolvedVersion;
        pkg.source = dep.isGit() ? dep.gitUrl : (dep.isPath() ? dep.path : "registry");
        pkg.commit = dep.resolvedCommit;
        pkg.checksum = dep.checksum;
        lf.packages[dep.name] = pkg;
    }

    return lf;
}

void DependencyResolver::addDependency(ProjectManifest& manifest, const DependencySpec& dep) {
    // Remove existing dep with same name
    manifest.dependencies.erase(
        std::remove_if(manifest.dependencies.begin(), manifest.dependencies.end(),
                       [&](const DependencySpec& d) { return d.name == dep.name; }),
        manifest.dependencies.end()
    );
    manifest.dependencies.push_back(dep);
}

void DependencyResolver::removeDependency(ProjectManifest& manifest, const std::string& name) {
    manifest.dependencies.erase(
        std::remove_if(manifest.dependencies.begin(), manifest.dependencies.end(),
                       [&](const DependencySpec& d) { return d.name == name; }),
        manifest.dependencies.end()
    );
}

void DependencyResolver::saveManifest(const ProjectManifest& manifest) {
    auto doc = manifest.toToml();
    TomlParser::writeFile(manifestPath, doc);
}
