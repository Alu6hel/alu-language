#pragma once
#include <string>
#include <vector>

enum class TokenType {
    TOK_EOF,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_INT_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_ROUTINE,
    TOK_EXTERN,
    TOK_EXPORT,
    TOK_STRUCT,
    TOK_RETURN,
    TOK_UNSAFE,
    TOK_ASM,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_TRY,
    TOK_CATCH,
    TOK_THROW,
    TOK_EFFECT,
    TOK_HANDLE,
    TOK_YIELD,
    TOK_RESUME,
    TOK_ON,
    TOK_IN,
    TOK_INT_TYPE,
    TOK_STRING_TYPE,
    TOK_BOOL_TYPE,
    TOK_FLOAT_TYPE,
    TOK_DOUBLE_TYPE,
    TOK_BYTE_TYPE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_EQUALS,
    TOK_DOUBLE_EQUALS,
    TOK_NOT_EQUALS,
    TOK_LESS_THAN,
    TOK_LESS_EQUALS,
    TOK_GREATER_THAN,
    TOK_GREATER_EQUALS,
    TOK_ARROW,
    TOK_PLUS,
    TOK_MINUS,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_DOT,
    TOK_ELLIPSIS,
    TOK_AMPERSAND,
    TOK_BIT_OR,
    TOK_BIT_XOR,
    TOK_BIT_NOT,
    TOK_LSHIFT,
    TOK_RSHIFT,
    TOK_STAR,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_NEW,
    TOK_FREE,
    TOK_COLON,
    TOK_IMPORT,
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
