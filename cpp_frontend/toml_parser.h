#pragma once
#include <string>
#include <map>
#include <vector>
#include <variant>
#include <optional>
#include <fstream>
#include <sstream>

// A lightweight TOML parser for alu.toml manifest files.
// Supports: string values, integers, booleans, tables, inline tables, arrays, and comments.

enum class TomlValueType {
    STRING,
    INTEGER,
    BOOLEAN,
    TABLE,
    INLINE_TABLE,
    ARRAY
};

struct TomlValue {
    TomlValueType type;
    std::string stringVal;
    int64_t intVal = 0;
    bool boolVal = false;
    std::map<std::string, TomlValue> tableVal;
    std::vector<TomlValue> arrayVal;

    TomlValue() : type(TomlValueType::STRING) {}

    static TomlValue String(const std::string& s) {
        TomlValue v;
        v.type = TomlValueType::STRING;
        v.stringVal = s;
        return v;
    }

    static TomlValue Integer(int64_t i) {
        TomlValue v;
        v.type = TomlValueType::INTEGER;
        v.intVal = i;
        return v;
    }

    static TomlValue Boolean(bool b) {
        TomlValue v;
        v.type = TomlValueType::BOOLEAN;
        v.boolVal = b;
        return v;
    }

    static TomlValue Table() {
        TomlValue v;
        v.type = TomlValueType::TABLE;
        return v;
    }

    static TomlValue InlineTable() {
        TomlValue v;
        v.type = TomlValueType::INLINE_TABLE;
        return v;
    }

    static TomlValue Array() {
        TomlValue v;
        v.type = TomlValueType::ARRAY;
        return v;
    }

    // Convenience accessors
    bool isString() const { return type == TomlValueType::STRING; }
    bool isTable() const { return type == TomlValueType::TABLE || type == TomlValueType::INLINE_TABLE; }
    bool isArray() const { return type == TomlValueType::ARRAY; }
    bool isInteger() const { return type == TomlValueType::INTEGER; }
    bool isBoolean() const { return type == TomlValueType::BOOLEAN; }

    // Get a nested value by key (for tables)
    const TomlValue* get(const std::string& key) const {
        if (!isTable()) return nullptr;
        auto it = tableVal.find(key);
        if (it == tableVal.end()) return nullptr;
        return &it->second;
    }

    // Get string value with default
    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        auto* v = get(key);
        if (v && v->isString()) return v->stringVal;
        return defaultVal;
    }
};

// The top-level TOML document is a table of tables
using TomlDocument = std::map<std::string, TomlValue>;

class TomlParser {
public:
    // Parse a TOML string into a document
    static TomlDocument parse(const std::string& content);

    // Parse a TOML file
    static TomlDocument parseFile(const std::string& filepath);

    // Serialize a document back to TOML string
    static std::string serialize(const TomlDocument& doc);

    // Write a document to a file
    static bool writeFile(const std::string& filepath, const TomlDocument& doc);

private:
    struct ParserState {
        const std::string& src;
        size_t pos;
        int line;

        ParserState(const std::string& s) : src(s), pos(0), line(1) {}

        char current() const { return pos < src.size() ? src[pos] : '\0'; }
        bool eof() const { return pos >= src.size(); }
        void advance() { if (pos < src.size()) { if (src[pos] == '\n') line++; pos++; } }
        char peek(int offset = 1) const { return (pos + offset < src.size()) ? src[pos + offset] : '\0'; }
    };

    static void skipWhitespace(ParserState& state);
    static void skipWhitespaceAndNewlines(ParserState& state);
    static void skipComment(ParserState& state);
    static void skipToEndOfLine(ParserState& state);
    static std::string parseKey(ParserState& state);
    static std::string parseQuotedString(ParserState& state);
    static TomlValue parseValue(ParserState& state);
    static TomlValue parseInlineTable(ParserState& state);
    static TomlValue parseArray(ParserState& state);
    static std::string parseTableHeader(ParserState& state);
};
