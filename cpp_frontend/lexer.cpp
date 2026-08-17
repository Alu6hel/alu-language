#include "lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(std::string src) : source(src), pos(0) {}

char Lexer::currentChar() {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

void Lexer::advance() {
    if (pos < source.length()) {
        if (source[pos] == '\n') {
            currentLine++;
            currentCol = 1;
        } else {
            currentCol++;
        }
    }
    pos++;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < source.length()) {
        if (isspace(currentChar())) {
            advance();
        } else if (currentChar() == '/' && pos + 1 < source.length() && source[pos+1] == '/') {
            // Skip single line comment
            while (currentChar() != '\n' && currentChar() != '\0') {
                advance();
            }
        } else {
            break;
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (pos < source.length()) {
        skipWhitespaceAndComments();
        if (currentChar() == '\0') break;

        int startLine = currentLine;
        int startCol = currentCol;
        
        char c = currentChar();
        
        if (isalpha(c) || c == '_') {
            std::string ident = "";
            while (isalnum(currentChar()) || currentChar() == '_') {
                ident += currentChar();
                advance();
            }
            
            if (ident == "routine") tokens.push_back({TokenType::TOK_ROUTINE, ident, startLine, startCol});
            else if (ident == "extern") tokens.push_back({TokenType::TOK_EXTERN, ident, startLine, startCol});
            else if (ident == "struct") tokens.push_back({TokenType::TOK_STRUCT, ident, startLine, startCol});
            else if (ident == "assert") tokens.push_back({TokenType::TOK_ASSERT, ident, startLine, startCol});
            else if (ident == "return") tokens.push_back({TokenType::TOK_RETURN, ident, startLine, startCol});
            else if (ident == "unsafe") tokens.push_back({TokenType::TOK_UNSAFE, ident, startLine, startCol});
            else if (ident == "asm") tokens.push_back({TokenType::TOK_ASM, ident, startLine, startCol});
            else if (ident == "if") tokens.push_back({TokenType::TOK_IF, ident, startLine, startCol});
            else if (ident == "else") tokens.push_back({TokenType::TOK_ELSE, ident, startLine, startCol});
            else if (ident == "while") tokens.push_back({TokenType::TOK_WHILE, ident, startLine, startCol});
            else if (ident == "for") tokens.push_back({TokenType::TOK_FOR, ident, startLine, startCol});
            else if (ident == "try") tokens.push_back({TokenType::TOK_TRY, ident, startLine, startCol});
            else if (ident == "catch") tokens.push_back({TokenType::TOK_CATCH, ident, startLine, startCol});
            else if (ident == "throw") tokens.push_back({TokenType::TOK_THROW, ident, startLine, startCol});
            else if (ident == "effect") tokens.push_back({TokenType::TOK_EFFECT, ident, startLine, startCol});
            else if (ident == "handle") tokens.push_back({TokenType::TOK_HANDLE, ident, startLine, startCol});
            else if (ident == "yield") tokens.push_back({TokenType::TOK_YIELD, ident, startLine, startCol});
            else if (ident == "resume") tokens.push_back({TokenType::TOK_RESUME, ident, startLine, startCol});
            else if (ident == "on") tokens.push_back({TokenType::TOK_ON, ident, startLine, startCol});
            else if (ident == "in") tokens.push_back({TokenType::TOK_IN, ident, startLine, startCol});
            else if (ident == "int") tokens.push_back({TokenType::TOK_INT_TYPE, ident, startLine, startCol});
            else if (ident == "string") tokens.push_back({TokenType::TOK_STRING_TYPE, ident, startLine, startCol});
            else if (ident == "bool") tokens.push_back({TokenType::TOK_BOOL_TYPE, ident, startLine, startCol});
            else if (ident == "float") tokens.push_back({TokenType::TOK_FLOAT_TYPE, ident, startLine, startCol});
            else if (ident == "double") tokens.push_back({TokenType::TOK_DOUBLE_TYPE, ident, startLine, startCol});
            else if (ident == "byte") tokens.push_back({TokenType::TOK_BYTE_TYPE, ident, startLine, startCol});
            else if (ident == "float4") tokens.push_back({TokenType::TOK_FLOAT4_TYPE, ident, startLine, startCol});
            else if (ident == "float8") tokens.push_back({TokenType::TOK_FLOAT8_TYPE, ident, startLine, startCol});
            else if (ident == "int4") tokens.push_back({TokenType::TOK_INT4_TYPE, ident, startLine, startCol});
            else if (ident == "int8") tokens.push_back({TokenType::TOK_INT8_TYPE, ident, startLine, startCol});
            else if (ident == "new") tokens.push_back({TokenType::TOK_NEW, ident, startLine, startCol});
            else if (ident == "free") tokens.push_back({TokenType::TOK_FREE, ident, startLine, startCol});
            else if (ident == "export") tokens.push_back({TokenType::TOK_EXPORT, ident, startLine, startCol});
            else if (ident == "import") tokens.push_back({TokenType::TOK_IMPORT, ident, startLine, startCol});
            else if (ident == "namespace") tokens.push_back({TokenType::TOK_NAMESPACE, ident, startLine, startCol});
            else if (ident == "as") tokens.push_back({TokenType::TOK_AS, ident, startLine, startCol});
            else tokens.push_back({TokenType::TOK_IDENTIFIER, ident, startLine, startCol});
        } 
        else if (isdigit(c)) {
            std::string num = "";
            while (isdigit(currentChar())) {
                num += currentChar();
                advance();
            }
            if (currentChar() == '.') {
                num += '.';
                advance();
                while (isdigit(currentChar())) {
                    num += currentChar();
                    advance();
                }
                tokens.push_back({TokenType::TOK_FLOAT_LITERAL, num, startLine, startCol});
            } else {
                tokens.push_back({TokenType::TOK_INT_LITERAL, num, startLine, startCol});
            }
        }
        else if (c == '"' || c == '\'') {
            char quoteType = c;
            advance(); // skip opening quote
            std::string str = "";
            while (currentChar() != quoteType && currentChar() != '\0') {
                str += currentChar();
                advance();
            }
            advance(); // skip closing quote
            tokens.push_back({TokenType::TOK_STRING, str, startLine, startCol});
        }
        else if (c == '(') { tokens.push_back({TokenType::TOK_LPAREN, "(", startLine, startCol}); advance(); }
        else if (c == ')') { tokens.push_back({TokenType::TOK_RPAREN, ")", startLine, startCol}); advance(); }
        else if (c == '{') { tokens.push_back({TokenType::TOK_LBRACE, "{", startLine, startCol}); advance(); }
        else if (c == '}') { tokens.push_back({TokenType::TOK_RBRACE, "}"}); advance(); }
        else if (c == '=') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_DOUBLE_EQUALS, "==", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_EQUALS, "=", startLine, startCol});
            }
        }
        else if (c == '!') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_NOT_EQUALS, "!=", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_UNKNOWN, "!", startLine, startCol});
            }
        }
        else if (c == '<') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_LESS_EQUALS, "<=", startLine, startCol});
                advance();
            } else if (currentChar() == '<') {
                tokens.push_back({TokenType::TOK_LSHIFT, "<<", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_LESS_THAN, "<", startLine, startCol});
            }
        }
        else if (c == '>') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_GREATER_EQUALS, ">=", startLine, startCol});
                advance();
            } else if (currentChar() == '>') {
                tokens.push_back({TokenType::TOK_RSHIFT, ">>", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_GREATER_THAN, ">", startLine, startCol});
            }
        }
        else if (c == '-') {
            advance();
            if (currentChar() == '>') {
                tokens.push_back({TokenType::TOK_ARROW, "->", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_MINUS, "-", startLine, startCol});
            }
        }
        else if (c == '+') { tokens.push_back({TokenType::TOK_PLUS, "+", startLine, startCol}); advance(); }
        else if (c == ';') { tokens.push_back({TokenType::TOK_SEMICOLON, ";", startLine, startCol}); advance(); }
        else if (c == ',') { tokens.push_back({TokenType::TOK_COMMA, ",", startLine, startCol}); advance(); }
        else if (c == '.') {
            advance();
            if (currentChar() == '.' && pos + 1 < source.length() && source[pos+1] == '.') {
                advance();
                advance();
                tokens.push_back({TokenType::TOK_ELLIPSIS, "...", startLine, startCol});
            } else {
                tokens.push_back({TokenType::TOK_DOT, ".", startLine, startCol});
            }
        }
        else if (c == '&') { tokens.push_back({TokenType::TOK_AMPERSAND, "&", startLine, startCol}); advance(); }
        else if (c == '|') { tokens.push_back({TokenType::TOK_BIT_OR, "|", startLine, startCol}); advance(); }
        else if (c == '^') { tokens.push_back({TokenType::TOK_BIT_XOR, "^", startLine, startCol}); advance(); }
        else if (c == '~') { tokens.push_back({TokenType::TOK_BIT_NOT, "~", startLine, startCol}); advance(); }
        else if (c == ':') {
            advance();
            if (currentChar() == ':') {
                tokens.push_back({TokenType::TOK_DOUBLE_COLON, "::", startLine, startCol});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_COLON, ":", startLine, startCol});
            }
        }
        else if (c == '*') { tokens.push_back({TokenType::TOK_STAR, "*", startLine, startCol}); advance(); }
        else if (c == '/') { tokens.push_back({TokenType::TOK_SLASH, "/", startLine, startCol}); advance(); }
        else if (c == '%') { tokens.push_back({TokenType::TOK_PERCENT, "%", startLine, startCol}); advance(); }
        else if (c == '[') { tokens.push_back({TokenType::TOK_LBRACKET, "[", startLine, startCol}); advance(); }
        else if (c == ']') { tokens.push_back({TokenType::TOK_RBRACKET, "]", startLine, startCol}); advance(); }
        else if (c == '@') {
            advance();
            std::string ident = "";
            while (isalpha(currentChar()) || currentChar() == '_') {
                ident += currentChar();
                advance();
            }
            if (ident == "requires") {
                tokens.push_back({TokenType::TOK_REQUIRES, "@requires", startLine, startCol});
            } else if (ident == "ensures") {
                tokens.push_back({TokenType::TOK_ENSURES, "@ensures", startLine, startCol});
            } else {
                tokens.push_back({TokenType::TOK_UNKNOWN, "@" + ident, startLine, startCol});
            }
        }
        else {
            std::string unknown = "";
            unknown += c;
            tokens.push_back({TokenType::TOK_UNKNOWN, unknown, startLine, startCol});
            advance();
        }
    }
    
    tokens.push_back({TokenType::TOK_EOF, "", currentLine, currentCol});
    return tokens;
}
