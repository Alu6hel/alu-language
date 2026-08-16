#pragma once
#include <string>
#include <vector>
#include <optional>

// Semantic Versioning (SemVer 2.0.0) parser and constraint resolver.
// Supports: MAJOR.MINOR.PATCH[-prerelease][+buildmeta]
// Constraints: ^1.2.3, ~1.2.3, >=1.0.0, <=2.0.0, =1.2.3, *

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;  // e.g., "alpha.1"
    std::string buildMeta;   // e.g., "build.123"

    SemVer() = default;
    SemVer(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    // Parse a version string like "1.2.3-alpha+build"
    static std::optional<SemVer> parse(const std::string& str);

    // Convert back to string
    std::string toString() const;

    // Comparison operators
    bool operator==(const SemVer& other) const;
    bool operator!=(const SemVer& other) const;
    bool operator<(const SemVer& other) const;
    bool operator<=(const SemVer& other) const;
    bool operator>(const SemVer& other) const;
    bool operator>=(const SemVer& other) const;

    // Is this a valid (non-zero) version?
    bool isValid() const { return major > 0 || minor > 0 || patch > 0 || !prerelease.empty(); }
};

// Constraint operators
enum class ConstraintOp {
    EXACT,           // =1.2.3 or just 1.2.3
    CARET,           // ^1.2.3 — compatible with 1.x.y where x >= 2
    TILDE,           // ~1.2.3 — patch-level changes: 1.2.x
    GREATER_EQUAL,   // >=1.0.0
    GREATER,         // >1.0.0
    LESS_EQUAL,      // <=2.0.0
    LESS,            // <2.0.0
    WILDCARD         // * — any version
};

struct VersionConstraint {
    ConstraintOp op = ConstraintOp::CARET;
    SemVer version;

    // Parse a constraint string like "^1.2.3", ">=1.0.0", "~1.2", etc.
    static std::optional<VersionConstraint> parse(const std::string& str);

    // Check if a given version satisfies this constraint
    bool satisfiedBy(const SemVer& candidate) const;

    // Convert back to string
    std::string toString() const;
};

// A compound constraint (AND of multiple constraints, e.g., ">=1.0.0, <2.0.0")
struct VersionRange {
    std::vector<VersionConstraint> constraints;

    // Parse a range string like ">=1.0.0, <2.0.0" or "^1.2.3"
    static VersionRange parse(const std::string& str);

    // Check if a version satisfies ALL constraints in this range
    bool satisfiedBy(const SemVer& candidate) const;

    // Find the best (highest) matching version from a list
    std::optional<SemVer> bestMatch(const std::vector<SemVer>& versions) const;
};
