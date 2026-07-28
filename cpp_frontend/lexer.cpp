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
            else if (ident == "unsafe") tokens.push_back({TokenType::TOK_UNSAFE, ident});
            else if (ident == "asm") tokens.push_back({TokenType::TOK_ASM, ident});
            else if (ident == "int") tokens.push_back({TokenType::TOK_INT_TYPE, ident});
            else if (ident == "string") tokens.push_back({TokenType::TOK_STRING_TYPE, ident});
            else tokens.push_back({TokenType::TOK_IDENTIFIER, ident});
        } 
        else if (isdigit(c)) {
            std::string num = "";
            while (isdigit(currentChar())) {
                num += currentChar();
                advance();
            }
            tokens.push_back({TokenType::TOK_INT_LITERAL, num});
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
        else if (c == '=') { tokens.push_back({TokenType::TOK_EQUALS, "="}); advance(); }
        else if (c == '+') { tokens.push_back({TokenType::TOK_PLUS, "+"}); advance(); }
        else if (c == ';') { tokens.push_back({TokenType::TOK_SEMICOLON, ";"}); advance(); }
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
