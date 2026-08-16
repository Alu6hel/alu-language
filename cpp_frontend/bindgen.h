#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

struct BindgenOptions {
    std::string inputFile;
    std::string outputFile;
    std::string namespaceName;
    std::string stripPrefix;
    std::vector<std::string> includeDirs;
    std::vector<std::string> predefines;
    std::set<std::string> opaqueTypes;
    std::vector<std::string> allowFunctions;
    std::vector<std::string> blockFunctions;
    bool generateWrapper = false;
    bool verbose = false;
};

struct CParam {
    std::string type;
    std::string name;
};

struct CFunction {
    std::string name;
    std::string returnType;
    std::vector<CParam> params;
    bool isVariadic = false;
    std::string docComment;
};

struct CStructField {
    std::string type;
    std::string name;
    int arraySize = 0; // > 0 if char buf[64]
};

struct CStruct {
    std::string name;
    std::vector<CStructField> fields;
    bool isOpaque = false;
    std::string docComment;
};

struct CEnumMember {
    std::string name;
    std::string value;
};

struct CEnum {
    std::string name;
    std::vector<CEnumMember> members;
    std::string docComment;
};

struct CTypedef {
    std::string alias;
    std::string targetType;
};

struct CMacro {
    std::string name;
    std::string value;
    bool isNumeric = false;
};

class CHeaderParser {
public:
    explicit CHeaderParser(const BindgenOptions& options);
    bool parse();

    const std::vector<CFunction>& getFunctions() const { return functions; }
    const std::vector<CStruct>& getStructs() const { return structs; }
    const std::vector<CEnum>& getEnums() const { return enums; }
    const std::vector<CTypedef>& getTypedefs() const { return typedefs; }
    const std::vector<CMacro>& getMacros() const { return macros; }

private:
    BindgenOptions options;
    std::vector<CFunction> functions;
    std::vector<CStruct> structs;
    std::vector<CEnum> enums;
    std::vector<CTypedef> typedefs;
    std::vector<CMacro> macros;
    std::set<std::string> parsedFiles;

    std::string readFileContent(const std::string& path);
    std::string preprocess(const std::string& source, const std::string& currentFilePath);
    void parseTokens(const std::string& code);
};

class AluBindingEmitter {
public:
    static std::string mapCTypeToAlu(const std::string& cType, const std::map<std::string, std::string>& typedefMap);
    static std::string generateAluBindings(const CHeaderParser& parser, const BindgenOptions& options);
};
