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
    TOK_ASSERT,
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
    TOK_NAMESPACE,
    TOK_DOUBLE_COLON,
    TOK_REQUIRES,
    TOK_ENSURES,
    TOK_AS,
    TOK_UNKNOWN
};

inline std::string TokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::TOK_EOF: return "EOF";
        case TokenType::TOK_IDENTIFIER: return "identifier";
        case TokenType::TOK_STRING: return "string literal";
        case TokenType::TOK_INT_LITERAL: return "int literal";
        case TokenType::TOK_FLOAT_LITERAL: return "float literal";
        case TokenType::TOK_ROUTINE: return "'routine'";
        case TokenType::TOK_EXTERN: return "'extern'";
        case TokenType::TOK_EXPORT: return "'export'";
        case TokenType::TOK_STRUCT: return "'struct'";
        case TokenType::TOK_ASSERT: return "'assert'";
        case TokenType::TOK_RETURN: return "'return'";
        case TokenType::TOK_UNSAFE: return "'unsafe'";
        case TokenType::TOK_ASM: return "'asm'";
        case TokenType::TOK_IF: return "'if'";
        case TokenType::TOK_ELSE: return "'else'";
        case TokenType::TOK_WHILE: return "'while'";
        case TokenType::TOK_FOR: return "'for'";
        case TokenType::TOK_TRY: return "'try'";
        case TokenType::TOK_CATCH: return "'catch'";
        case TokenType::TOK_THROW: return "'throw'";
        case TokenType::TOK_EFFECT: return "'effect'";
        case TokenType::TOK_HANDLE: return "'handle'";
        case TokenType::TOK_YIELD: return "'yield'";
        case TokenType::TOK_RESUME: return "'resume'";
        case TokenType::TOK_ON: return "'on'";
        case TokenType::TOK_IN: return "'in'";
        case TokenType::TOK_INT_TYPE: return "'int'";
        case TokenType::TOK_STRING_TYPE: return "'string'";
        case TokenType::TOK_BOOL_TYPE: return "'bool'";
        case TokenType::TOK_FLOAT_TYPE: return "'float'";
        case TokenType::TOK_DOUBLE_TYPE: return "'double'";
        case TokenType::TOK_BYTE_TYPE: return "'byte'";
        case TokenType::TOK_LPAREN: return "'('";
        case TokenType::TOK_RPAREN: return "')'";
        case TokenType::TOK_LBRACE: return "'{'";
        case TokenType::TOK_RBRACE: return "'}'";
        case TokenType::TOK_EQUALS: return "'='";
        case TokenType::TOK_DOUBLE_EQUALS: return "'=='";
        case TokenType::TOK_NOT_EQUALS: return "'!='";
        case TokenType::TOK_LESS_THAN: return "'<'";
        case TokenType::TOK_LESS_EQUALS: return "'<='";
        case TokenType::TOK_GREATER_THAN: return "'>'";
        case TokenType::TOK_GREATER_EQUALS: return "'>='";
        case TokenType::TOK_ARROW: return "'->'";
        case TokenType::TOK_PLUS: return "'+'";
        case TokenType::TOK_MINUS: return "'-'";
        case TokenType::TOK_SLASH: return "'/'";
        case TokenType::TOK_PERCENT: return "'%'";
        case TokenType::TOK_SEMICOLON: return "';'";
        case TokenType::TOK_COMMA: return "','";
        case TokenType::TOK_DOT: return "'.'";
        case TokenType::TOK_ELLIPSIS: return "'...'";
        case TokenType::TOK_AMPERSAND: return "'&'";
        case TokenType::TOK_BIT_OR: return "'|'";
        case TokenType::TOK_BIT_XOR: return "'^'";
        case TokenType::TOK_BIT_NOT: return "'~'";
        case TokenType::TOK_LSHIFT: return "'<<'";
        case TokenType::TOK_RSHIFT: return "'>>'";
        case TokenType::TOK_STAR: return "'*'";
        case TokenType::TOK_LBRACKET: return "'['";
        case TokenType::TOK_RBRACKET: return "']'";
        case TokenType::TOK_NEW: return "'new'";
        case TokenType::TOK_FREE: return "'free'";
        case TokenType::TOK_COLON: return "':'";
        case TokenType::TOK_IMPORT: return "'import'";
        case TokenType::TOK_NAMESPACE: return "'namespace'";
        case TokenType::TOK_DOUBLE_COLON: return "'::'";
        case TokenType::TOK_REQUIRES: return "'@requires'";
        case TokenType::TOK_ENSURES: return "'@ensures'";
        case TokenType::TOK_AS: return "'as'";
        default: return "unknown token";
    }
}

struct Token {
    TokenType type;
    std::string value;
    int line = 1;
    int col = 1;
};

class Lexer {
private:
    std::string source;
    size_t pos;
    int currentLine = 1;
    int currentCol = 1;
    char currentChar();
    void advance();
    void skipWhitespaceAndComments();
public:
    Lexer(std::string src);
    std::vector<Token> tokenize();
};
