#include "lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(std::string src) : source(src), pos(0) {}

char Lexer::currentChar() {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

void Lexer::advance() {
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

        char c = currentChar();
        
        if (isalpha(c) || c == '_') {
            std::string ident = "";
            while (isalnum(currentChar()) || currentChar() == '_') {
                ident += currentChar();
                advance();
            }
            
            if (ident == "routine") tokens.push_back({TokenType::TOK_ROUTINE, ident});
            else if (ident == "extern") tokens.push_back({TokenType::TOK_EXTERN, ident});
            else if (ident == "struct") tokens.push_back({TokenType::TOK_STRUCT, ident});
            else if (ident == "return") tokens.push_back({TokenType::TOK_RETURN, ident});
            else if (ident == "unsafe") tokens.push_back({TokenType::TOK_UNSAFE, ident});
            else if (ident == "asm") tokens.push_back({TokenType::TOK_ASM, ident});
            else if (ident == "if") tokens.push_back({TokenType::TOK_IF, ident});
            else if (ident == "else") tokens.push_back({TokenType::TOK_ELSE, ident});
            else if (ident == "while") tokens.push_back({TokenType::TOK_WHILE, ident});
            else if (ident == "for") tokens.push_back({TokenType::TOK_FOR, ident});
            else if (ident == "try") tokens.push_back({TokenType::TOK_TRY, ident});
            else if (ident == "catch") tokens.push_back({TokenType::TOK_CATCH, ident});
            else if (ident == "throw") tokens.push_back({TokenType::TOK_THROW, ident});
            else if (ident == "effect") tokens.push_back({TokenType::TOK_EFFECT, ident});
            else if (ident == "handle") tokens.push_back({TokenType::TOK_HANDLE, ident});
            else if (ident == "yield") tokens.push_back({TokenType::TOK_YIELD, ident});
            else if (ident == "resume") tokens.push_back({TokenType::TOK_RESUME, ident});
            else if (ident == "on") tokens.push_back({TokenType::TOK_ON, ident});
            else if (ident == "in") tokens.push_back({TokenType::TOK_IN, ident});
            else if (ident == "int") tokens.push_back({TokenType::TOK_INT_TYPE, ident});
            else if (ident == "string") tokens.push_back({TokenType::TOK_STRING_TYPE, ident});
            else if (ident == "bool") tokens.push_back({TokenType::TOK_BOOL_TYPE, ident});
            else if (ident == "float") tokens.push_back({TokenType::TOK_FLOAT_TYPE, ident});
            else if (ident == "double") tokens.push_back({TokenType::TOK_DOUBLE_TYPE, ident});
            else if (ident == "byte") tokens.push_back({TokenType::TOK_BYTE_TYPE, ident});
            else if (ident == "new") tokens.push_back({TokenType::TOK_NEW, ident});
            else if (ident == "export") tokens.push_back({TokenType::TOK_EXPORT, ident});
            else if (ident == "import") tokens.push_back({TokenType::TOK_IMPORT, ident});
            else if (ident == "namespace") tokens.push_back({TokenType::TOK_NAMESPACE, ident});
            else tokens.push_back({TokenType::TOK_IDENTIFIER, ident});
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
                tokens.push_back({TokenType::TOK_FLOAT_LITERAL, num});
            } else {
                tokens.push_back({TokenType::TOK_INT_LITERAL, num});
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
            tokens.push_back({TokenType::TOK_STRING, str});
        }
        else if (c == '(') { tokens.push_back({TokenType::TOK_LPAREN, "("}); advance(); }
        else if (c == ')') { tokens.push_back({TokenType::TOK_RPAREN, ")"}); advance(); }
        else if (c == '{') { tokens.push_back({TokenType::TOK_LBRACE, "{"}); advance(); }
        else if (c == '}') { tokens.push_back({TokenType::TOK_RBRACE, "}"}); advance(); }
        else if (c == '=') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_DOUBLE_EQUALS, "=="});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_EQUALS, "="});
            }
        }
        else if (c == '!') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_NOT_EQUALS, "!="});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_UNKNOWN, "!"});
            }
        }
        else if (c == '<') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_LESS_EQUALS, "<="});
                advance();
            } else if (currentChar() == '<') {
                tokens.push_back({TokenType::TOK_LSHIFT, "<<"});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_LESS_THAN, "<"});
            }
        }
        else if (c == '>') {
            advance();
            if (currentChar() == '=') {
                tokens.push_back({TokenType::TOK_GREATER_EQUALS, ">="});
                advance();
            } else if (currentChar() == '>') {
                tokens.push_back({TokenType::TOK_RSHIFT, ">>"});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_GREATER_THAN, ">"});
            }
        }
        else if (c == '-') {
            advance();
            if (currentChar() == '>') {
                tokens.push_back({TokenType::TOK_ARROW, "->"});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_MINUS, "-"});
            }
        }
        else if (c == '+') { tokens.push_back({TokenType::TOK_PLUS, "+"}); advance(); }
        else if (c == ';') { tokens.push_back({TokenType::TOK_SEMICOLON, ";"}); advance(); }
        else if (c == ',') { tokens.push_back({TokenType::TOK_COMMA, ","}); advance(); }
        else if (c == '.') {
            advance();
            if (currentChar() == '.' && pos + 1 < source.length() && source[pos+1] == '.') {
                advance();
                advance();
                tokens.push_back({TokenType::TOK_ELLIPSIS, "..."});
            } else {
                tokens.push_back({TokenType::TOK_DOT, "."});
            }
        }
        else if (c == '&') { tokens.push_back({TokenType::TOK_AMPERSAND, "&"}); advance(); }
        else if (c == '|') { tokens.push_back({TokenType::TOK_BIT_OR, "|"}); advance(); }
        else if (c == '^') { tokens.push_back({TokenType::TOK_BIT_XOR, "^"}); advance(); }
        else if (c == '~') { tokens.push_back({TokenType::TOK_BIT_NOT, "~"}); advance(); }
        else if (c == ':') {
            advance();
            if (currentChar() == ':') {
                tokens.push_back({TokenType::TOK_DOUBLE_COLON, "::"});
                advance();
            } else {
                tokens.push_back({TokenType::TOK_COLON, ":"});
            }
        }
        else if (c == '*') { tokens.push_back({TokenType::TOK_STAR, "*"}); advance(); }
        else if (c == '/') { tokens.push_back({TokenType::TOK_SLASH, "/"}); advance(); }
        else if (c == '%') { tokens.push_back({TokenType::TOK_PERCENT, "%"}); advance(); }
        else if (c == '[') { tokens.push_back({TokenType::TOK_LBRACKET, "["}); advance(); }
        else if (c == ']') { tokens.push_back({TokenType::TOK_RBRACKET, "]"}); advance(); }
        else if (c == '@') {
            advance();
            std::string ident = "";
            while (isalpha(currentChar()) || currentChar() == '_') {
                ident += currentChar();
                advance();
            }
            if (ident == "requires") {
                tokens.push_back({TokenType::TOK_REQUIRES, "@requires"});
            } else if (ident == "ensures") {
                tokens.push_back({TokenType::TOK_ENSURES, "@ensures"});
            } else {
                tokens.push_back({TokenType::TOK_UNKNOWN, "@" + ident});
            }
        }
        else {
            std::string unknown = "";
            unknown += c;
            tokens.push_back({TokenType::TOK_UNKNOWN, unknown});
            advance();
        }
    }
    
    tokens.push_back({TokenType::TOK_EOF, ""});
    return tokens;
}
