import sys

content = open('cpp_frontend/parser.cpp').read()
content = content.replace(
    'if (currentToken().type != TokenType::TOK_IDENTIFIER) {', 
    'if (currentToken().type != TokenType::TOK_IDENTIFIER && currentToken().type != TokenType::TOK_STRING_TYPE && currentToken().type != TokenType::TOK_INT_TYPE && currentToken().type != TokenType::TOK_FLOAT_TYPE) {'
)
open('cpp_frontend/parser.cpp', 'w').write(content)
