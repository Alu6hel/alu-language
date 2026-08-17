#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>
#include <sstream>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "json.hpp"
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "z3_verifier.h"
#include "linker.h"

std::string globalStdPath = "../std";

std::string uriToPath(const std::string& uri) {
    std::string path = uri;
    const std::string prefix = "file:///";
    if (path.find(prefix) == 0) {
        path = path.substr(prefix.length());
    } else if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    size_t pos = 0;
    while ((pos = path.find("%3A", pos)) != std::string::npos) {
        path.replace(pos, 3, ":");
        pos += 1;
    }
    return path;
}


using json = nlohmann::json;


struct DocumentState {
    std::string text;
    std::shared_ptr<ProgramNode> ast;
    std::shared_ptr<SemanticAnalyzer> analyzer;
};

std::unordered_map<std::string, DocumentState> documents;


void sendResponse(const json& msg) {
    std::string s = msg.dump();
    std::cout << "Content-Length: " << s.length() << "\r\n\r\n" << s;
    std::cout.flush();
}

json createDiagnostic(int line, int col, const std::string& message) {
    // LSP lines and cols are 0-indexed. ALU compiler is 1-indexed.
    return {
        {"range", {
            {"start", {{"line", std::max(0, line - 1)}, {"character", std::max(0, col - 1)}}},
            {"end", {{"line", std::max(0, line - 1)}, {"character", std::max(0, col - 1) + 5}}}
        }},
        {"severity", 1}, // Error
        {"source", "alu-lsp"},
        {"message", message}
    };
}

void validateDocument(const std::string& uri, const std::string& content) {
    std::vector<json> diagnostics;
    
    DocumentState state;
    state.text = content;
    
    try {
        Lexer lexer(content);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(tokens, uri);
        state.ast = parser.parse();
        
        std::string localPath = uriToPath(uri);
        std::string currentDir = ModuleLinker::getDirectory(localPath);
        
        ModuleLinker linker(globalStdPath);
        linker.link(state.ast.get(), currentDir, localPath);
        
        state.analyzer = std::make_shared<SemanticAnalyzer>();
        state.analyzer->is_lsp_mode = true;
        state.analyzer->analyze(state.ast.get());
        
        Z3Verifier z3Verifier;
        z3Verifier.verify(state.ast.get());
    } catch (const std::exception& e) {

        std::cerr << "[DEBUG] Caught exception: " << e.what() << std::endl;
        std::string err = e.what();
        // Parse "test.alu:5:10: error: MSG" or "[ALU CXX] Compile Error in test.alu:5:10"
        std::regex re(":(.*?):(\\d+):(\\d+)(?:\\r?\\n)(.*)");
        std::smatch match;
        if (std::regex_search(err, match, re)) {
            int line = std::stoi(match[2].str());
            int col = std::stoi(match[3].str());
            std::string msg = match[4].str();
            diagnostics.push_back(createDiagnostic(line, col, msg));
        } else {
            diagnostics.push_back(createDiagnostic(1, 1, err));
        }
    }
    
    std::cerr << "[DEBUG] Saving document state..." << std::endl;

    
    documents[uri] = state;
    
    json params = {
        {"uri", uri},
        {"diagnostics", diagnostics}
    };
    std::cerr << "[DEBUG] Sending diagnostic response..." << std::endl;

    sendResponse({
                {"jsonrpc", "2.0"},
                {"method", "textDocument/publishDiagnostics"},
        {"params", params}
    });
}

void handleMessage(const json& msg) {
    if (msg.contains("method")) {
        std::string method = msg["method"];
        if (method == "initialize") {
            json result = {
                {"capabilities", {
                    {"textDocumentSync", 1}, // Full sync
                    {"completionProvider", {
                        {"resolveProvider", false},
                        {"triggerCharacters", {".", ":"}}
                    }},
                    {"hoverProvider", true},
                    {"definitionProvider", true},
                    {"semanticTokensProvider", {
                        {"legend", {
                            {"tokenTypes", {"keyword", "type", "variable", "string", "number", "operator"}},
                            {"tokenModifiers", {}}
                        }},
                        {"full", true}
                    }}
                }}
            };
            sendResponse({
                {"jsonrpc", "2.0"},
                {"id", msg["id"]},
                {"result", result}
            });
        } else if (method == "textDocument/didOpen") {
            std::string uri = msg["params"]["textDocument"]["uri"];
            std::string text = msg["params"]["textDocument"]["text"];
            validateDocument(uri, text);
        } else if (method == "textDocument/didChange") {
            std::string uri = msg["params"]["textDocument"]["uri"];
            std::string text = msg["params"]["contentChanges"][0]["text"];
            validateDocument(uri, text);
        } else if (method == "textDocument/semanticTokens/full") {
            std::string uri = msg["params"]["textDocument"]["uri"];
            std::string text = documents[uri].text;
            
            Lexer lexer(text);
            std::vector<Token> tokens;
            try { tokens = lexer.tokenize(); } catch (...) {}
            
            std::vector<int> data;
            int prevLine = 1;
            int prevCol = 1;
            
            for (const auto& tok : tokens) {
                if (tok.type == TokenType::TOK_EOF) continue;
                int typeIdx = 2; // variable default
                if (tok.type == TokenType::TOK_ROUTINE || tok.type == TokenType::TOK_IF || tok.type == TokenType::TOK_ELSE) typeIdx = 0; // keyword
                else if (tok.type == TokenType::TOK_INT_TYPE || tok.type == TokenType::TOK_STRING_TYPE) typeIdx = 1; // type
                else if (tok.type == TokenType::TOK_STRING) typeIdx = 3; // string
                else if (tok.type == TokenType::TOK_INT_LITERAL || tok.type == TokenType::TOK_FLOAT_LITERAL) typeIdx = 4; // number
                else if (tok.type == TokenType::TOK_PLUS || tok.type == TokenType::TOK_MINUS) typeIdx = 5; // operator
                
                int deltaLine = tok.line - prevLine;
                int deltaCol = (deltaLine == 0) ? (tok.col - prevCol) : (tok.col - 1);
                
                data.push_back(deltaLine);
                data.push_back(deltaCol);
                data.push_back(tok.value.length());
                data.push_back(typeIdx);
                data.push_back(0); // modifier
                
                prevLine = tok.line;
                prevCol = tok.col;
            }
            sendResponse({
                {"jsonrpc", "2.0"},
                {"id", msg["id"]},
                {"result", {{"data", data}}}
            });
        } else if (method == "textDocument/hover") {
            std::string uri = msg["params"]["textDocument"]["uri"];
            int req_line = (int)msg["params"]["position"]["line"] + 1; // LSP is 0-indexed
            int req_col = (int)msg["params"]["position"]["character"] + 1;
            
            json result = nullptr;
            if (documents.find(uri) != documents.end() && documents[uri].analyzer) {
                auto& symbols = documents[uri].analyzer->lsp_symbols;
                for (const auto& sym : symbols) {
                    if (sym.line == req_line && req_col >= sym.col && req_col <= sym.col + sym.length) {
                        result = {
                            {"contents", {
                                {"kind", "markdown"},
                                {"value", sym.hover_text}
                            }},
                            {"range", {
                                {"start", {{"line", sym.line - 1}, {"character", sym.col - 1}}},
                                {"end", {{"line", sym.line - 1}, {"character", sym.col - 1 + sym.length}}}
                            }}
                        };
                        break;
                    }
                }
            }
            sendResponse({{"jsonrpc", "2.0"}, {"id", msg["id"]}, {"result", result}});
        } else if (method == "textDocument/definition") {
            std::string uri = msg["params"]["textDocument"]["uri"];
            int req_line = (int)msg["params"]["position"]["line"] + 1;
            int req_col = (int)msg["params"]["position"]["character"] + 1;
            
            json result = nullptr;
            if (documents.find(uri) != documents.end() && documents[uri].analyzer) {
                auto& symbols = documents[uri].analyzer->lsp_symbols;
                for (const auto& sym : symbols) {
                    if (sym.line == req_line && req_col >= sym.col && req_col <= sym.col + sym.length) {
                        if (sym.def_line > 0 && !sym.def_file.empty()) {
                            std::string fixed_uri = sym.def_file;
                            if (fixed_uri.find("file://") != 0) {
                                fixed_uri = "file:///" + fixed_uri; // Basic heuristic
                            }
                            result = {
                                {"uri", fixed_uri},
                                {"range", {
                                    {"start", {{"line", sym.def_line - 1}, {"character", sym.def_col - 1}}},
                                    {"end", {{"line", sym.def_line - 1}, {"character", sym.def_col - 1 + sym.name.length()}}}
                                }}
                            };
                        }
                        break;
                    }
                }
            }
            sendResponse({{"jsonrpc", "2.0"}, {"id", msg["id"]}, {"result", result}});
        } else if (method == "textDocument/completion") {
            json items = json::array();
            items.push_back({{"label", "routine"}, {"kind", 14}});
            items.push_back({{"label", "struct"}, {"kind", 14}});
            items.push_back({{"label", "int"}, {"kind", 14}});
            items.push_back({{"label", "string"}, {"kind", 14}});
            
            // Add dynamically collected symbols from semantic analyzer if available
            std::string uri = msg["params"]["textDocument"]["uri"];
            if (documents.find(uri) != documents.end() && documents[uri].analyzer) {
                auto& symbols = documents[uri].analyzer->lsp_symbols;
                for (const auto& sym : symbols) {
                    if (sym.hover_text.find("routine") != std::string::npos) {
                        items.push_back({{"label", sym.name}, {"kind", 3}}); // Function
                    } else {
                        items.push_back({{"label", sym.name}, {"kind", 6}}); // Variable
                    }
                }
            }
            
            sendResponse({
                {"jsonrpc", "2.0"},
                {"id", msg["id"]},
                {"result", items}
            });
        } else if (method == "shutdown") {
            sendResponse({
                {"jsonrpc", "2.0"},
                {"id", msg["id"]},
                {"result", nullptr}
            });
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 0) {
        globalStdPath = ModuleLinker::getDirectory(argv[0]) + "/../std";
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.length() >= 14 && line.substr(0, 14) == "Content-Length") {
            int length = 0;
            try {
                length = std::stoi(line.substr(16));
            } catch (...) { continue; }
            std::getline(std::cin, line); // empty line
            std::vector<char> buffer(length + 1, 0);
            std::cin.read(buffer.data(), length);
            
            try {
                json msg = json::parse(buffer.data());
                handleMessage(msg);
            } catch (const std::exception& e) {

                std::cerr << "[DEBUG] Caught exception: " << e.what() << std::endl;
                // Ignore parsing errors for now
            }
        }
    }
    return 0;
}
