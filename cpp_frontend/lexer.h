#pragma once
#include <string>
#include <vector>

enum class TokenType {
    TOK_EOF,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_INT_LITERAL,
    TOK_ROUTINE,
    TOK_UNSAFE,
    TOK_ASM,
    TOK_INT_TYPE,
    TOK_STRING_TYPE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_EQUALS,
    TOK_PLUS,
    TOK_SEMICOLON,
    TOK_UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
};

class Lexer {
private:
    std::string source;
    size_t pos;
    char currentChar();
    void advance();
    void skipWhitespaceAndComments();
public:
    Lexer(std::string src);
    std::vector<Token> tokenize();
};
