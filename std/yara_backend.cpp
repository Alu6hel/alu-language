#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

extern "C" {

struct YaraRule {
    std::vector<std::vector<uint8_t>> patterns;
};

static std::vector<YaraRule> rules;

int yara_create_rule() {
    int id = rules.size();
    rules.push_back(YaraRule());
    return id;
}

void yara_add_string_pattern(int rule_id, const char* str) {
    if (rule_id >= 0 && rule_id < rules.size() && str != nullptr) {
        std::string s(str);
        std::vector<uint8_t> pat(s.begin(), s.end());
        rules[rule_id].patterns.push_back(pat);
    }
}

static int hex2int(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

void yara_add_hex_pattern(int rule_id, const char* hex_str) {
    if (rule_id >= 0 && rule_id < rules.size() && hex_str != nullptr) {
        std::vector<uint8_t> pat;
        std::string hs(hex_str);
        for (size_t i = 0; i < hs.length();) {
            if (hs[i] == ' ' || hs[i] == '\n' || hs[i] == '\r' || hs[i] == '\t') {
                i++;
                continue;
            }
            if (i + 1 < hs.length()) {
                int h1 = hex2int(hs[i]);
                int h2 = hex2int(hs[i+1]);
                if (h1 >= 0 && h2 >= 0) {
                    pat.push_back((uint8_t)((h1 << 4) | h2));
                }
                i += 2;
            } else {
                break;
            }
        }
        if (!pat.empty()) {
            rules[rule_id].patterns.push_back(pat);
        }
    }
}

int yara_scan_buffer(int rule_id, const char* data, int length) {
    if (rule_id < 0 || rule_id >= rules.size() || data == nullptr || length <= 0) return 0;
    const uint8_t* udata = reinterpret_cast<const uint8_t*>(data);
    
    // We treat patterns as OR matching (if any pattern matches, return 1)
    for (const auto& pat : rules[rule_id].patterns) {
        if (pat.empty()) continue;
        auto it = std::search(udata, udata + length, pat.begin(), pat.end());
        if (it != udata + length) {
            return 1; // Match found
        }
    }
    return 0;
}

int yara_scan_file(int rule_id, const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[YARA] Failed to open file for scanning: " << filepath << std::endl;
        return 0;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();
    return yara_scan_buffer(rule_id, data.c_str(), data.length());
}

} // extern "C"
