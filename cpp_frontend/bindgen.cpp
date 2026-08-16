#include "bindgen.h"
#include <regex>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

CHeaderParser::CHeaderParser(const BindgenOptions& opts) : options(opts) {}

std::string CHeaderParser::readFileContent(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (options.verbose) {
            std::cerr << "[bindgen] Warning: Could not open file " << path << std::endl;
        }
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string CHeaderParser::preprocess(const std::string& source, const std::string& currentFilePath) {
    std::stringstream in(source);
    std::stringstream out;
    std::string line;

    while (std::getline(in, line)) {
        std::string trimmed = trimString(line);
        if (trimmed.rfind("#define", 0) == 0) {
            // Extract numeric or string macro definitions
            std::stringstream ls(trimmed);
            std::string directive, name, val;
            ls >> directive >> name;
            std::getline(ls, val);
            val = trimString(val);
            if (!name.empty() && !val.empty()) {
                // Check if numeric, hex, or simple constant
                bool isNum = false;
                if (!val.empty() && (std::isdigit(val[0]) || val[0] == '-' || val.rfind("0x", 0) == 0 || val.rfind("0X", 0) == 0)) {
                    isNum = true;
                }
                // Strip trailing L, U, UL, ULL
                std::string cleanVal = val;
                while (!cleanVal.empty() && (cleanVal.back() == 'U' || cleanVal.back() == 'u' || cleanVal.back() == 'L' || cleanVal.back() == 'l' || cleanVal.back() == 'F' || cleanVal.back() == 'f')) {
                    cleanVal.pop_back();
                }
                if (cleanVal.empty()) cleanVal = val;

                if (isNum) {
                    CMacro macro;
                    macro.name = name;
                    macro.value = cleanVal;
                    macro.isNumeric = true;
                    macros.push_back(macro);
                }
            }
            continue;
        } else if (trimmed.rfind("#include", 0) == 0) {
            // Check if we should process included local headers
            size_t q1 = trimmed.find('"');
            size_t q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                std::string incFile = trimmed.substr(q1 + 1, q2 - q1 - 1);
                fs::path parentDir = fs::path(currentFilePath).parent_path();
                fs::path targetPath = parentDir / incFile;
                
                if (!fs::exists(targetPath)) {
                    for (const auto& incDir : options.includeDirs) {
                        fs::path candidate = fs::path(incDir) / incFile;
                        if (fs::exists(candidate)) {
                            targetPath = candidate;
                            break;
                        }
                    }
                }

                std::string absPath = fs::absolute(targetPath).string();
                if (parsedFiles.find(absPath) == parsedFiles.end() && fs::exists(targetPath)) {
                    parsedFiles.insert(absPath);
                    if (options.verbose) {
                        std::cout << "[bindgen] Including header: " << absPath << std::endl;
                    }
                    std::string incContent = readFileContent(absPath);
                    out << preprocess(incContent, absPath) << "\n";
                }
            }
            continue;
        } else if (!trimmed.empty() && trimmed[0] == '#') {
            // Strip other preprocessor directives (#pragma, #ifdef, etc.)
            continue;
        }
        out << line << "\n";
    }
    return out.str();
}

bool CHeaderParser::parse() {
    parsedFiles.insert(fs::absolute(options.inputFile).string());
    std::string rawSource = readFileContent(options.inputFile);
    if (rawSource.empty()) {
        std::cerr << "[bindgen] Error: Input header file is empty or could not be read: " << options.inputFile << std::endl;
        return false;
    }

    std::string cleanSource = preprocess(rawSource, options.inputFile);
    parseTokens(cleanSource);
    return true;
}

void CHeaderParser::parseTokens(const std::string& code) {
    // Strip compiler extensions and annotations
    std::string text = code;
    text = std::regex_replace(text, std::regex(R"(__attribute__\s*\(\([^)]*\)\))"), "");
    text = std::regex_replace(text, std::regex(R"(__declspec\s*\(\([^)]*\)\))"), "");
    text = std::regex_replace(text, std::regex(R"(__declspec\s*\([^)]*\))"), "");
    text = std::regex_replace(text, std::regex(R"(\bWINAPI\b)"), "");
    text = std::regex_replace(text, std::regex(R"(\bAPIENTRY\b)"), "");
    text = std::regex_replace(text, std::regex(R"(\b__cdecl\b)"), "");
    text = std::regex_replace(text, std::regex(R"(\b__stdcall\b)"), "");
    text = std::regex_replace(text, std::regex(R"(\bextern\s+"C"\s*\{)"), "");
    text = std::regex_replace(text, std::regex(R"(\bextern\s+"C"\b)"), "");

    // Strip comments while preserving doc comments
    std::string noComments;
    bool inMultiComment = false;
    bool inSingleComment = false;
    std::string currentDoc = "";

    for (size_t i = 0; i < text.length(); ++i) {
        if (inMultiComment) {
            if (i + 1 < text.length() && text[i] == '*' && text[i+1] == '/') {
                inMultiComment = false;
                i++;
            }
            continue;
        }
        if (inSingleComment) {
            if (text[i] == '\n') {
                inSingleComment = false;
                noComments += '\n';
            }
            continue;
        }
        if (i + 1 < text.length() && text[i] == '/' && text[i+1] == '*') {
            inMultiComment = true;
            i++;
            continue;
        }
        if (i + 1 < text.length() && text[i] == '/' && text[i+1] == '/') {
            inSingleComment = true;
            i++;
            continue;
        }
        noComments += text[i];
    }

    // Tokenize into statements ended by ';' or '}'
    std::stringstream ss(noComments);
    std::string token;
    std::vector<std::string> statements;
    std::string currentStmt;
    int braceDepth = 0;

    for (char c : noComments) {
        currentStmt += c;
        if (c == '{') braceDepth++;
        else if (c == '}') braceDepth--;

        if (c == ';' && braceDepth == 0) {
            std::string s = trimString(currentStmt);
            if (!s.empty()) {
                statements.push_back(s);
            }
            currentStmt.clear();
        }
    }
    if (!trimString(currentStmt).empty()) {
        statements.push_back(trimString(currentStmt));
    }

    for (const auto& stmt : statements) {
        std::string s = trimString(stmt);
        if (s.empty()) continue;

        // Parse Typedef Struct or Standard Struct
        if (s.rfind("typedef struct", 0) == 0 || s.rfind("struct", 0) == 0) {
            size_t braceStart = s.find('{');
            size_t braceEnd = s.rfind('}');
            if (braceStart != std::string::npos && braceEnd != std::string::npos && braceEnd > braceStart) {
                std::string header = trimString(s.substr(0, braceStart));
                std::string body = s.substr(braceStart + 1, braceEnd - braceStart - 1);
                std::string footer = trimString(s.substr(braceEnd + 1));
                if (!footer.empty() && footer.back() == ';') footer.pop_back();
                footer = trimString(footer);

                std::string structName;
                std::stringstream hs(header);
                std::vector<std::string> hTokens;
                std::string htok;
                while (hs >> htok) hTokens.push_back(htok);

                if (hTokens.size() >= 2 && hTokens[0] == "struct") {
                    structName = hTokens[1];
                } else if (hTokens.size() >= 3 && hTokens[0] == "typedef" && hTokens[1] == "struct") {
                    structName = hTokens[2];
                }
                if (structName.empty()) {
                    structName = footer;
                }
                if (structName.empty()) continue;

                CStruct cs;
                cs.name = structName;

                // Parse struct body fields
                std::stringstream bs(body);
                std::string fieldLine;
                while (std::getline(bs, fieldLine, ';')) {
                    std::string f = trimString(fieldLine);
                    if (f.empty()) continue;

                    size_t lastSpace = f.find_last_of(" \t*");
                    if (lastSpace != std::string::npos) {
                        std::string fType = trimString(f.substr(0, lastSpace + 1));
                        std::string fName = trimString(f.substr(lastSpace + 1));
                        
                        // Check inline array e.g. char name[64]
                        int arrSize = 0;
                        size_t b1 = fName.find('[');
                        size_t b2 = fName.find(']');
                        if (b1 != std::string::npos && b2 != std::string::npos && b2 > b1) {
                            std::string sizeStr = fName.substr(b1 + 1, b2 - b1 - 1);
                            try { arrSize = std::stoi(sizeStr); } catch(...) {}
                            fName = trimString(fName.substr(0, b1));
                        }

                        CStructField sf;
                        sf.type = fType;
                        sf.name = fName;
                        sf.arraySize = arrSize;
                        cs.fields.push_back(sf);
                    }
                }
                structs.push_back(cs);
                continue;
            } else {
                // Forward declaration e.g. struct OpaqueContext;
                std::stringstream ss(s);
                std::string kw, name;
                ss >> kw >> name;
                if (!name.empty() && name.back() == ';') name.pop_back();
                if (!name.empty()) {
                    CStruct cs;
                    cs.name = name;
                    cs.isOpaque = true;
                    structs.push_back(cs);
                }
                continue;
            }
        }

        // Parse Typedef Enum or Enum
        if (s.rfind("typedef enum", 0) == 0 || s.rfind("enum", 0) == 0) {
            size_t braceStart = s.find('{');
            size_t braceEnd = s.rfind('}');
            if (braceStart != std::string::npos && braceEnd != std::string::npos && braceEnd > braceStart) {
                std::string header = trimString(s.substr(0, braceStart));
                std::string body = s.substr(braceStart + 1, braceEnd - braceStart - 1);
                std::string footer = trimString(s.substr(braceEnd + 1));
                if (!footer.empty() && footer.back() == ';') footer.pop_back();
                footer = trimString(footer);

                std::string enumName;
                std::stringstream hs(header);
                std::vector<std::string> hTokens;
                std::string htok;
                while (hs >> htok) hTokens.push_back(htok);

                if (hTokens.size() >= 2 && hTokens[0] == "enum") {
                    enumName = hTokens[1];
                } else if (hTokens.size() >= 3 && hTokens[0] == "typedef" && hTokens[1] == "enum") {
                    enumName = hTokens[2];
                }
                if (enumName.empty()) {
                    enumName = footer;
                }
                if (enumName.empty()) enumName = "AnonymousEnum";

                CEnum ce;
                ce.name = enumName;

                std::stringstream bs(body);
                std::string item;
                int defaultVal = 0;

                while (std::getline(bs, item, ',')) {
                    item = trimString(item);
                    if (item.empty()) continue;

                    size_t eqPos = item.find('=');
                    std::string mName, mVal;
                    if (eqPos != std::string::npos) {
                        mName = trimString(item.substr(0, eqPos));
                        mVal = trimString(item.substr(eqPos + 1));
                        try { defaultVal = std::stoi(mVal) + 1; } catch(...) { defaultVal++; }
                    } else {
                        mName = item;
                        mVal = std::to_string(defaultVal++);
                    }

                    CEnumMember mem;
                    mem.name = mName;
                    mem.value = mVal;
                    ce.members.push_back(mem);
                }
                enums.push_back(ce);
                continue;
            }
        }

        // Parse Typedef
        if (s.rfind("typedef", 0) == 0) {
            std::string t = s;
            if (!t.empty() && t.back() == ';') t.pop_back();
            std::stringstream ts(t);
            std::string kw, target, alias;
            ts >> kw;
            std::vector<std::string> parts;
            std::string p;
            while (ts >> p) parts.push_back(p);
            if (parts.size() >= 2) {
                alias = parts.back();
                for (size_t i = 0; i < parts.size() - 1; ++i) {
                    target += parts[i] + (i < parts.size() - 2 ? " " : "");
                }
                CTypedef td;
                td.alias = alias;
                td.targetType = target;
                typedefs.push_back(td);
            }
            continue;
        }

        // Parse Function Prototypes (must contain parentheses and return type)
        size_t parenStart = s.find('(');
        size_t parenEnd = s.rfind(')');
        if (parenStart != std::string::npos && parenEnd != std::string::npos && parenEnd > parenStart) {
            std::string left = trimString(s.substr(0, parenStart));
            std::string argsStr = s.substr(parenStart + 1, parenEnd - parenStart - 1);

            size_t lastSpace = left.find_last_of(" \t*");
            if (lastSpace != std::string::npos) {
                std::string retType = trimString(left.substr(0, lastSpace + 1));
                std::string funcName = trimString(left.substr(lastSpace + 1));

                if (funcName.empty() || funcName == "if" || funcName == "while" || funcName == "for" || funcName == "return") {
                    continue;
                }

                // Check allow / block function filters
                if (!options.allowFunctions.empty()) {
                    bool allowed = false;
                    for (const auto& pat : options.allowFunctions) {
                        if (funcName.find(pat) != std::string::npos) { allowed = true; break; }
                    }
                    if (!allowed) continue;
                }

                bool blocked = false;
                for (const auto& pat : options.blockFunctions) {
                    if (funcName.find(pat) != std::string::npos) { blocked = true; break; }
                }
                if (blocked) continue;

                CFunction cf;
                cf.name = funcName;
                cf.returnType = retType;

                // Parse function arguments
                std::stringstream as(argsStr);
                std::string arg;
                while (std::getline(as, arg, ',')) {
                    arg = trimString(arg);
                    if (arg.empty() || arg == "void") continue;

                    if (arg == "...") {
                        cf.isVariadic = true;
                        continue;
                    }

                    size_t argSpace = arg.find_last_of(" \t*");
                    std::string aType, aName;
                    if (argSpace != std::string::npos) {
                        aType = trimString(arg.substr(0, argSpace + 1));
                        aName = trimString(arg.substr(argSpace + 1));
                    } else {
                        aType = arg;
                        aName = "arg" + std::to_string(cf.params.size() + 1);
                    }

                    CParam cp;
                    cp.type = aType;
                    cp.name = aName;
                    cf.params.push_back(cp);
                }
                functions.push_back(cf);
            }
        }
    }
}

// Map C types to Alu types
std::string AluBindingEmitter::mapCTypeToAlu(const std::string& cType, const std::map<std::string, std::string>& typedefMap) {
    std::string t = cType;
    
    // Strip qualifiers
    t = std::regex_replace(t, std::regex(R"(\bconst\b)"), "");
    t = std::regex_replace(t, std::regex(R"(\bvolatile\b)"), "");
    t = std::regex_replace(t, std::regex(R"(\bstruct\b)"), "");
    t = std::regex_replace(t, std::regex(R"(\benum\b)"), "");
    t = std::regex_replace(t, std::regex(R"(\bunion\b)"), "");
    t = std::regex_replace(t, std::regex(R"(\binline\b)"), "");
    t = trimString(t);

    if (typedefMap.count(t)) {
        t = typedefMap.at(t);
    }

    // String / Char pointer mappings
    if (t == "char*" || t == "char *" || t == "const char*" || t == "const char *") return "string";

    // Pointer types
    if (t.back() == '*') {
        std::string base = trimString(t.substr(0, t.length() - 1));
        std::string mappedBase = mapCTypeToAlu(base, typedefMap);
        if (mappedBase == "void") return "void*";
        if (mappedBase == "string") return "ptr<u8>";
        return "ptr<" + mappedBase + ">";
    }

    // Primitives
    if (t == "void") return "void";
    if (t == "char" || t == "signed char" || t == "int8_t") return "i8";
    if (t == "unsigned char" || t == "uint8_t" || t == "u8") return "u8";
    if (t == "short" || t == "signed short" || t == "int16_t") return "i16";
    if (t == "unsigned short" || t == "uint16_t" || t == "u16") return "u16";
    if (t == "int" || t == "signed int" || t == "int32_t" || t == "long" || t == "signed long") return "int";
    if (t == "unsigned int" || t == "uint32_t" || t == "u32" || t == "unsigned long") return "u32";
    if (t == "long long" || t == "int64_t" || t == "intmax_t" || t == "ptrdiff_t") return "i64";
    if (t == "unsigned long long" || t == "uint64_t" || t == "size_t" || t == "uintptr_t") return "u64";
    if (t == "float") return "float";
    if (t == "double") return "double";
    if (t == "bool" || t == "_Bool") return "bool";

    return t.empty() ? "void" : t;
}

std::string AluBindingEmitter::generateAluBindings(const CHeaderParser& parser, const BindgenOptions& options) {
    std::stringstream out;

    out << "// ──────────────────────────────────────────────────────────────────────────────\n";
    out << "// Auto-generated by alu_bindgen for Alu Language\n";
    out << "// Source Header: " << options.inputFile << "\n";
    out << "// ──────────────────────────────────────────────────────────────────────────────\n\n";

    bool inNamespace = !options.namespaceName.empty();
    std::string indent = inNamespace ? "    " : "";

    if (inNamespace) {
        out << "namespace " << options.namespaceName << " {\n";
    }

    // 1. Emit Macros
    if (!parser.getMacros().empty()) {
        out << indent << "// Preprocessor Constants\n";
        out << indent << "namespace Constants {\n";
        for (const auto& m : parser.getMacros()) {
            out << indent << "    let " << m.name << ": int = " << m.value << ";\n";
        }
        out << indent << "}\n\n";
    }

    // 2. Emit Enums
    for (const auto& e : parser.getEnums()) {
        out << indent << "// Enum: " << e.name << "\n";
        out << indent << "namespace " << e.name << " {\n";
        for (const auto& mem : e.members) {
            out << indent << "    let " << mem.name << ": int = " << mem.value << ";\n";
        }
        out << indent << "}\n\n";
    }

    // Build typedef lookup map
    std::map<std::string, std::string> typedefMap;
    for (const auto& td : parser.getTypedefs()) {
        typedefMap[td.alias] = td.targetType;
    }

    // 3. Emit Structs
    for (const auto& s : parser.getStructs()) {
        out << indent << "struct " << s.name << " {\n";
        if (s.isOpaque || s.fields.empty()) {
            out << indent << "    // Opaque struct representation\n";
            out << indent << "    u8 _unused;\n";
        } else {
            for (const auto& f : s.fields) {
                std::string aluType = mapCTypeToAlu(f.type, typedefMap);
                if (f.arraySize > 0) {
                    aluType = "ptr<" + aluType + ">";
                }
                out << indent << "    " << aluType << " " << f.name << ";\n";
            }
        }
        out << indent << "}\n\n";
    }

    // 4. Emit Extern Routines
    out << indent << "// Extern C Routines\n";
    for (const auto& fn : parser.getFunctions()) {
        std::string name = fn.name;
        if (!options.stripPrefix.empty() && name.rfind(options.stripPrefix, 0) == 0) {
            name = name.substr(options.stripPrefix.length());
        }

        std::string retType = mapCTypeToAlu(fn.returnType, typedefMap);
        out << indent << "extern routine " << name << "(";

        for (size_t i = 0; i < fn.params.size(); ++i) {
            std::string pType = mapCTypeToAlu(fn.params[i].type, typedefMap);
            out << pType << " " << fn.params[i].name;
            if (i < fn.params.size() - 1) out << ", ";
        }
        if (fn.isVariadic) {
            if (!fn.params.empty()) out << ", ";
            out << "...";
        }
        out << ") -> " << retType << ";\n";
    }

    // 5. High-Level Wrapper Routines (Optional)
    if (options.generateWrapper) {
        out << "\n" << indent << "// High-Level Idiomatic Wrappers\n";
        for (const auto& fn : parser.getFunctions()) {
            std::string name = fn.name;
            if (!options.stripPrefix.empty() && name.rfind(options.stripPrefix, 0) == 0) {
                name = name.substr(options.stripPrefix.length());
            }
            std::string retType = mapCTypeToAlu(fn.returnType, typedefMap);

            out << indent << "routine safe_" << name << "(";
            for (size_t i = 0; i < fn.params.size(); ++i) {
                std::string pType = mapCTypeToAlu(fn.params[i].type, typedefMap);
                out << fn.params[i].name << ": " << pType;
                if (i < fn.params.size() - 1) out << ", ";
            }
            out << ") -> " << retType << " {\n";

            // Null pointer assertions for pointer types
            for (const auto& p : fn.params) {
                std::string pType = mapCTypeToAlu(p.type, typedefMap);
                if (pType.find("ptr<") != std::string::npos || pType == "string" || pType == "void*") {
                    out << indent << "    assert(" << p.name << " != null);\n";
                }
            }

            out << indent << "    ";
            if (retType != "void") out << "return ";
            out << name << "(";
            for (size_t i = 0; i < fn.params.size(); ++i) {
                out << fn.params[i].name;
                if (i < fn.params.size() - 1) out << ", ";
            }
            out << ");\n";
            out << indent << "}\n";
        }
    }

    if (inNamespace) {
        out << "}\n";
    }

    return out.str();
}
