import re

with open('cpp_frontend/parser.cpp', 'r') as f:
    content = f.read()

target = '''    } else if (currentToken().type == TokenType::TOK_INT_TYPE || currentToken().type == TokenType::TOK_STRING_TYPE 
|| currentToken().type == TokenType::TOK_BOOL_TYPE || currentToken().type == TokenType::TOK_FLOAT_TYPE || 
currentToken().type == TokenType::TOK_DOUBLE_TYPE || currentToken().type == TokenType::TOK_BYTE_TYPE) {
        return parseVarDecl();'''
target = target.replace('\n', '') # It might be wrapped

# Let's use a simpler regex
content = re.sub(
    r'currentToken\(\)\.type == TokenType::TOK_BYTE_TYPE\)',
    r'currentToken().type == TokenType::TOK_BYTE_TYPE || currentToken().type == TokenType::TOK_FLOAT4_TYPE || currentToken().type == TokenType::TOK_FLOAT8_TYPE || currentToken().type == TokenType::TOK_INT4_TYPE || currentToken().type == TokenType::TOK_INT8_TYPE)',
    content
)

with open('cpp_frontend/parser.cpp', 'w') as f:
    f.write(content)

print("Parser var decl fixed")
