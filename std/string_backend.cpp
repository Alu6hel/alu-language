#include <string>
#include <regex>
#include <cstring>
#include <cstdlib>

extern "C" void* alu_alloc(size_t size);

extern "C" char* alu_string_substr(const char* s, int start, int len) {
    if (!s) return nullptr;
    std::string str(s);
    if (start < 0 || start >= str.length()) return nullptr;
    std::string sub = str.substr(start, len);
    char* res = (char*)alu_alloc(sub.length() + 1);
    strcpy(res, sub.c_str());
    return res;
}

extern "C" char* alu_string_replace(const char* s, const char* search, const char* replace) {
    if (!s || !search || !replace) return nullptr;
    std::string str(s);
    std::string srch(search);
    std::string rplc(replace);
    
    size_t pos = 0;
    while ((pos = str.find(srch, pos)) != std::string::npos) {
        str.replace(pos, srch.length(), rplc);
        pos += rplc.length();
    }
    
    char* res = (char*)alu_alloc(str.length() + 1);
    strcpy(res, str.c_str());
    return res;
}

extern "C" int alu_string_find(const char* s, const char* search, int start) {
    if (!s || !search) return -1;
    std::string str(s);
    std::string srch(search);
    if (start < 0 || start > str.length()) return -1;
    size_t pos = str.find(srch, start);
    if (pos == std::string::npos) return -1;
    return (int)pos;
}

extern "C" int alu_regex_match(const char* s, const char* pattern) {
    if (!s || !pattern) return 0;
    try {
        std::regex re(pattern);
        return std::regex_match(s, re) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

extern "C" int alu_regex_search(const char* s, const char* pattern) {
    if (!s || !pattern) return 0;
    try {
        std::regex re(pattern);
        return std::regex_search(s, re) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

extern "C" char* alu_regex_replace(const char* s, const char* pattern, const char* replace) {
    if (!s || !pattern || !replace) return nullptr;
    try {
        std::regex re(pattern);
        std::string res_str = std::regex_replace(s, re, replace);
        char* res = (char*)alu_alloc(res_str.length() + 1);
        strcpy(res, res_str.c_str());
        return res;
    } catch (...) {
        char* res = (char*)alu_alloc(strlen(s) + 1);
        strcpy(res, s);
        return res;
    }
}

extern "C" int read_char_c(const char* s, int idx) {
    if (!s) return 0;
    return s[idx];
}

#include <iostream>

extern "C" void alu_print(const char* msg) {
    if (msg) {
        std::cout << msg << std::endl;
    }
}

extern "C" int alu_str_eq(const char* a, const char* b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}
