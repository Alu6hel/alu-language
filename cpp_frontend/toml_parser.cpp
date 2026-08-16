#include "toml_parser.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>

// ─── Whitespace / Comment Helpers ────────────────────────────────────────────

void TomlParser::skipWhitespace(ParserState& state) {
    while (!state.eof() && (state.current() == ' ' || state.current() == '\t')) {
        state.advance();
    }
}

void TomlParser::skipWhitespaceAndNewlines(ParserState& state) {
    while (!state.eof()) {
        char c = state.current();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            state.advance();
        } else if (c == '#') {
            skipComment(state);
        } else {
            break;
        }
    }
}

void TomlParser::skipComment(ParserState& state) {
    if (state.current() == '#') {
        while (!state.eof() && state.current() != '\n') {
            state.advance();
        }
    }
}

void TomlParser::skipToEndOfLine(ParserState& state) {
    skipWhitespace(state);
    skipComment(state);
    // Consume the newline if present
    if (!state.eof() && (state.current() == '\n' || state.current() == '\r')) {
        if (state.current() == '\r') state.advance();
        if (!state.eof() && state.current() == '\n') state.advance();
    }
}

// ─── Key Parsing ─────────────────────────────────────────────────────────────

std::string TomlParser::parseKey(ParserState& state) {
    skipWhitespace(state);

    if (state.current() == '"') {
        return parseQuotedString(state);
    }

    std::string key;
    while (!state.eof()) {
        char c = state.current();
        if (c == '=' || c == ' ' || c == '\t' || c == ']' || c == '.' || c == '\n' || c == '\r') {
            break;
        }
        key += c;
        state.advance();
    }

    if (key.empty()) {
        throw std::runtime_error("TOML parse error: expected key at line " + std::to_string(state.line));
    }
    return key;
}

// ─── String Parsing ──────────────────────────────────────────────────────────

std::string TomlParser::parseQuotedString(ParserState& state) {
    char quote = state.current(); // '"' or '\''
    state.advance(); // consume opening quote

    // Check for triple-quoted (multi-line) strings
    if (state.current() == quote && state.peek() == quote) {
        // Triple-quoted string — skip for now, treat as empty
        state.advance();
        state.advance();
        std::string result;
        while (!state.eof()) {
            if (state.current() == quote && state.peek() == quote && state.peek(2) == quote) {
                state.advance();
                state.advance();
                state.advance();
                return result;
            }
            result += state.current();
            state.advance();
        }
        return result;
    }

    std::string result;
    while (!state.eof() && state.current() != quote) {
        if (state.current() == '\\' && quote == '"') {
            state.advance(); // consume backslash
            switch (state.current()) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += '\\'; result += state.current(); break;
            }
        } else {
            result += state.current();
        }
        state.advance();
    }

    if (state.eof()) {
        throw std::runtime_error("TOML parse error: unterminated string at line " + std::to_string(state.line));
    }
    state.advance(); // consume closing quote
    return result;
}

// ─── Value Parsing ───────────────────────────────────────────────────────────

TomlValue TomlParser::parseValue(ParserState& state) {
    skipWhitespace(state);

    if (state.eof()) {
        throw std::runtime_error("TOML parse error: unexpected end of input");
    }

    char c = state.current();

    // Quoted string
    if (c == '"' || c == '\'') {
        return TomlValue::String(parseQuotedString(state));
    }

    // Inline table
    if (c == '{') {
        return parseInlineTable(state);
    }

    // Array
    if (c == '[') {
        return parseArray(state);
    }

    // Boolean or bare value
    // Check for true/false
    if (state.src.substr(state.pos, 4) == "true") {
        state.pos += 4;
        return TomlValue::Boolean(true);
    }
    if (state.src.substr(state.pos, 5) == "false") {
        state.pos += 5;
        return TomlValue::Boolean(false);
    }

    // Integer (or possibly a bare string — we'll try integer first)
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
        std::string numStr;
        if (c == '-' || c == '+') { numStr += c; state.advance(); }
        while (!state.eof() && state.current() >= '0' && state.current() <= '9') {
            numStr += state.current();
            state.advance();
        }
        // Check this isn't actually a float (we store as string for simplicity)
        if (!state.eof() && state.current() == '.') {
            // Float — store as string
            numStr += '.';
            state.advance();
            while (!state.eof() && state.current() >= '0' && state.current() <= '9') {
                numStr += state.current();
                state.advance();
            }
            return TomlValue::String(numStr);
        }
        try {
            return TomlValue::Integer(std::stoll(numStr));
        } catch (...) {
            return TomlValue::String(numStr);
        }
    }

    // Bare string value (until end of line or comma)
    std::string bareVal;
    while (!state.eof() && state.current() != '\n' && state.current() != '\r' && 
           state.current() != '#' && state.current() != ',' && state.current() != '}' &&
           state.current() != ']') {
        bareVal += state.current();
        state.advance();
    }
    // Trim trailing whitespace
    while (!bareVal.empty() && (bareVal.back() == ' ' || bareVal.back() == '\t')) {
        bareVal.pop_back();
    }
    return TomlValue::String(bareVal);
}

// ─── Inline Table ────────────────────────────────────────────────────────────

TomlValue TomlParser::parseInlineTable(ParserState& state) {
    state.advance(); // consume '{'
    auto table = TomlValue::InlineTable();

    skipWhitespace(state);
    while (!state.eof() && state.current() != '}') {
        skipWhitespace(state);
        std::string key = parseKey(state);
        skipWhitespace(state);

        if (state.current() != '=') {
            throw std::runtime_error("TOML parse error: expected '=' after key '" + key + "' at line " + std::to_string(state.line));
        }
        state.advance(); // consume '='

        table.tableVal[key] = parseValue(state);

        skipWhitespace(state);
        if (state.current() == ',') {
            state.advance();
        }
        skipWhitespace(state);
    }

    if (state.current() == '}') {
        state.advance(); // consume '}'
    }
    return table;
}

// ─── Array ───────────────────────────────────────────────────────────────────

TomlValue TomlParser::parseArray(ParserState& state) {
    state.advance(); // consume '['
    auto arr = TomlValue::Array();

    skipWhitespaceAndNewlines(state);
    while (!state.eof() && state.current() != ']') {
        arr.arrayVal.push_back(parseValue(state));
        skipWhitespaceAndNewlines(state);
        if (state.current() == ',') {
            state.advance();
            skipWhitespaceAndNewlines(state);
        }
    }

    if (state.current() == ']') {
        state.advance(); // consume ']'
    }
    return arr;
}

// ─── Table Header ────────────────────────────────────────────────────────────

std::string TomlParser::parseTableHeader(ParserState& state) {
    state.advance(); // consume '['
    skipWhitespace(state);

    std::string header;
    while (!state.eof() && state.current() != ']') {
        header += state.current();
        state.advance();
    }

    if (state.current() == ']') {
        state.advance(); // consume ']'
    }

    // Trim whitespace
    while (!header.empty() && header.back() == ' ') header.pop_back();
    while (!header.empty() && header.front() == ' ') header.erase(header.begin());

    return header;
}

// ─── Main Parser ─────────────────────────────────────────────────────────────

TomlDocument TomlParser::parse(const std::string& content) {
    ParserState state(content);
    TomlDocument doc;
    std::string currentSection = "";

    while (!state.eof()) {
        skipWhitespaceAndNewlines(state);
        if (state.eof()) break;

        char c = state.current();

        // Comment line
        if (c == '#') {
            skipComment(state);
            continue;
        }

        // Table header
        if (c == '[') {
            currentSection = parseTableHeader(state);
            if (doc.find(currentSection) == doc.end()) {
                doc[currentSection] = TomlValue::Table();
            }
            skipToEndOfLine(state);
            continue;
        }

        // Key-value pair
        std::string key = parseKey(state);
        skipWhitespace(state);

        if (state.current() != '=') {
            throw std::runtime_error("TOML parse error: expected '=' after key '" + key + "' at line " + std::to_string(state.line));
        }
        state.advance(); // consume '='

        TomlValue value = parseValue(state);
        skipToEndOfLine(state);

        if (currentSection.empty()) {
            // Top-level key — store in a special "" section or as a root-level table
            // We'll create a root table entry
            if (doc.find("") == doc.end()) {
                doc[""] = TomlValue::Table();
            }
            doc[""].tableVal[key] = value;
        } else {
            doc[currentSection].tableVal[key] = value;
        }
    }

    return doc;
}

TomlDocument TomlParser::parseFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open TOML file: " + filepath);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return parse(ss.str());
}

// ─── Serializer ──────────────────────────────────────────────────────────────

static std::string escapeString(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            default: result += c; break;
        }
    }
    return result;
}

static std::string serializeValue(const TomlValue& val) {
    switch (val.type) {
        case TomlValueType::STRING:
            return "\"" + escapeString(val.stringVal) + "\"";
        case TomlValueType::INTEGER:
            return std::to_string(val.intVal);
        case TomlValueType::BOOLEAN:
            return val.boolVal ? "true" : "false";
        case TomlValueType::INLINE_TABLE: {
            std::string result = "{ ";
            bool first = true;
            for (const auto& [k, v] : val.tableVal) {
                if (!first) result += ", ";
                result += k + " = " + serializeValue(v);
                first = false;
            }
            result += " }";
            return result;
        }
        case TomlValueType::ARRAY: {
            std::string result = "[";
            bool first = true;
            for (const auto& item : val.arrayVal) {
                if (!first) result += ", ";
                result += serializeValue(item);
                first = false;
            }
            result += "]";
            return result;
        }
        case TomlValueType::TABLE:
            // Tables at the value level are serialized as inline tables
            return serializeValue(TomlValue::InlineTable());
        default:
            return "\"\"";
    }
}

std::string TomlParser::serialize(const TomlDocument& doc) {
    std::string result;

    // First, output root-level keys (keys under "" section)
    auto rootIt = doc.find("");
    if (rootIt != doc.end() && rootIt->second.isTable()) {
        for (const auto& [key, val] : rootIt->second.tableVal) {
            result += key + " = " + serializeValue(val) + "\n";
        }
        if (!rootIt->second.tableVal.empty()) {
            result += "\n";
        }
    }

    // Then output each section
    for (const auto& [section, tableVal] : doc) {
        if (section.empty()) continue; // already handled

        result += "[" + section + "]\n";
        if (tableVal.isTable()) {
            for (const auto& [key, val] : tableVal.tableVal) {
                result += key + " = " + serializeValue(val) + "\n";
            }
        }
        result += "\n";
    }

    return result;
}

bool TomlParser::writeFile(const std::string& filepath, const TomlDocument& doc) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << serialize(doc);
    file.close();
    return true;
}
