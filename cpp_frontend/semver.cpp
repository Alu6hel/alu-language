#include "semver.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

// ─── SemVer Parsing ──────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::optional<SemVer> SemVer::parse(const std::string& input) {
    std::string str = trim(input);
    if (str.empty()) return std::nullopt;

    // Strip leading 'v' or 'V'
    if (str[0] == 'v' || str[0] == 'V') {
        str = str.substr(1);
    }

    SemVer ver;

    // Split off build metadata (+...)
    auto plusPos = str.find('+');
    if (plusPos != std::string::npos) {
        ver.buildMeta = str.substr(plusPos + 1);
        str = str.substr(0, plusPos);
    }

    // Split off prerelease (-...)
    auto dashPos = str.find('-');
    if (dashPos != std::string::npos) {
        ver.prerelease = str.substr(dashPos + 1);
        str = str.substr(0, dashPos);
    }

    // Parse MAJOR.MINOR.PATCH
    std::istringstream ss(str);
    std::string part;
    std::vector<int> parts;

    while (std::getline(ss, part, '.')) {
        try {
            parts.push_back(std::stoi(part));
        } catch (...) {
            return std::nullopt;
        }
    }

    if (parts.empty() || parts.size() > 3) return std::nullopt;

    ver.major = parts[0];
    ver.minor = parts.size() > 1 ? parts[1] : 0;
    ver.patch = parts.size() > 2 ? parts[2] : 0;

    return ver;
}

std::string SemVer::toString() const {
    std::string result = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) result += "-" + prerelease;
    if (!buildMeta.empty()) result += "+" + buildMeta;
    return result;
}

// ─── SemVer Comparison ──────────────────────────────────────────────────────

bool SemVer::operator==(const SemVer& other) const {
    return major == other.major && minor == other.minor && patch == other.patch && prerelease == other.prerelease;
}

bool SemVer::operator!=(const SemVer& other) const {
    return !(*this == other);
}

bool SemVer::operator<(const SemVer& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;

    // Pre-release versions have LOWER precedence than release
    if (prerelease.empty() && !other.prerelease.empty()) return false;
    if (!prerelease.empty() && other.prerelease.empty()) return true;

    return prerelease < other.prerelease;
}

bool SemVer::operator<=(const SemVer& other) const { return *this < other || *this == other; }
bool SemVer::operator>(const SemVer& other) const { return other < *this; }
bool SemVer::operator>=(const SemVer& other) const { return other <= *this; }

// ─── VersionConstraint Parsing ───────────────────────────────────────────────

std::optional<VersionConstraint> VersionConstraint::parse(const std::string& input) {
    std::string str = trim(input);
    if (str.empty()) return std::nullopt;

    VersionConstraint constraint;

    if (str == "*") {
        constraint.op = ConstraintOp::WILDCARD;
        constraint.version = SemVer(0, 0, 0);
        return constraint;
    }

    // Detect operator prefix
    if (str.size() >= 2 && str[0] == '>' && str[1] == '=') {
        constraint.op = ConstraintOp::GREATER_EQUAL;
        str = str.substr(2);
    } else if (str.size() >= 2 && str[0] == '<' && str[1] == '=') {
        constraint.op = ConstraintOp::LESS_EQUAL;
        str = str.substr(2);
    } else if (str[0] == '>') {
        constraint.op = ConstraintOp::GREATER;
        str = str.substr(1);
    } else if (str[0] == '<') {
        constraint.op = ConstraintOp::LESS;
        str = str.substr(1);
    } else if (str[0] == '~') {
        constraint.op = ConstraintOp::TILDE;
        str = str.substr(1);
    } else if (str[0] == '^') {
        constraint.op = ConstraintOp::CARET;
        str = str.substr(1);
    } else if (str[0] == '=') {
        constraint.op = ConstraintOp::EXACT;
        str = str.substr(1);
    } else {
        // No operator — default to caret for Alu compatibility
        constraint.op = ConstraintOp::CARET;
    }

    str = trim(str);
    auto ver = SemVer::parse(str);
    if (!ver) return std::nullopt;

    constraint.version = *ver;
    return constraint;
}

bool VersionConstraint::satisfiedBy(const SemVer& candidate) const {
    switch (op) {
        case ConstraintOp::WILDCARD:
            return true;

        case ConstraintOp::EXACT:
            return candidate == version;

        case ConstraintOp::GREATER_EQUAL:
            return candidate >= version;

        case ConstraintOp::GREATER:
            return candidate > version;

        case ConstraintOp::LESS_EQUAL:
            return candidate <= version;

        case ConstraintOp::LESS:
            return candidate < version;

        case ConstraintOp::TILDE:
            // ~1.2.3 means >=1.2.3 and <1.3.0
            return candidate >= version &&
                   candidate.major == version.major &&
                   candidate.minor == version.minor;

        case ConstraintOp::CARET:
            // ^1.2.3 means >=1.2.3 and <2.0.0
            // ^0.2.3 means >=0.2.3 and <0.3.0
            // ^0.0.3 means >=0.0.3 and <0.0.4
            if (candidate < version) return false;
            if (version.major > 0) {
                return candidate.major == version.major;
            } else if (version.minor > 0) {
                return candidate.major == 0 && candidate.minor == version.minor;
            } else {
                return candidate.major == 0 && candidate.minor == 0 && candidate.patch == version.patch;
            }
    }
    return false;
}

std::string VersionConstraint::toString() const {
    std::string prefix;
    switch (op) {
        case ConstraintOp::EXACT:         prefix = "="; break;
        case ConstraintOp::CARET:         prefix = "^"; break;
        case ConstraintOp::TILDE:         prefix = "~"; break;
        case ConstraintOp::GREATER_EQUAL: prefix = ">="; break;
        case ConstraintOp::GREATER:       prefix = ">"; break;
        case ConstraintOp::LESS_EQUAL:    prefix = "<="; break;
        case ConstraintOp::LESS:          prefix = "<"; break;
        case ConstraintOp::WILDCARD:      return "*";
    }
    return prefix + version.toString();
}

// ─── VersionRange ────────────────────────────────────────────────────────────

VersionRange VersionRange::parse(const std::string& input) {
    VersionRange range;
    std::string str = trim(input);

    // Split on comma
    std::istringstream ss(str);
    std::string part;
    while (std::getline(ss, part, ',')) {
        part = trim(part);
        if (part.empty()) continue;
        auto constraint = VersionConstraint::parse(part);
        if (constraint) {
            range.constraints.push_back(*constraint);
        }
    }

    // If empty, treat as wildcard
    if (range.constraints.empty()) {
        VersionConstraint wc;
        wc.op = ConstraintOp::WILDCARD;
        range.constraints.push_back(wc);
    }

    return range;
}

bool VersionRange::satisfiedBy(const SemVer& candidate) const {
    for (const auto& constraint : constraints) {
        if (!constraint.satisfiedBy(candidate)) return false;
    }
    return true;
}

std::optional<SemVer> VersionRange::bestMatch(const std::vector<SemVer>& versions) const {
    std::optional<SemVer> best;
    for (const auto& v : versions) {
        if (satisfiedBy(v)) {
            if (!best || v > *best) {
                best = v;
            }
        }
    }
    return best;
}
