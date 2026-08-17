import re

# Update lexer.h
with open('cpp_frontend/lexer.h', 'r') as f:
    h_content = f.read()

h_content = h_content.replace(
'''    TOK_DOUBLE_TYPE,
    TOK_BYTE_TYPE,''',
'''    TOK_DOUBLE_TYPE,
    TOK_BYTE_TYPE,
    TOK_FLOAT4_TYPE,
    TOK_FLOAT8_TYPE,
    TOK_INT4_TYPE,
    TOK_INT8_TYPE,'''
)

h_content = h_content.replace(
'''        case TokenType::TOK_BYTE_TYPE: return "'byte'";''',
'''        case TokenType::TOK_BYTE_TYPE: return "'byte'";
        case TokenType::TOK_FLOAT4_TYPE: return "'float4'";
        case TokenType::TOK_FLOAT8_TYPE: return "'float8'";
        case TokenType::TOK_INT4_TYPE: return "'int4'";
        case TokenType::TOK_INT8_TYPE: return "'int8'";'''
)

with open('cpp_frontend/lexer.h', 'w') as f:
    f.write(h_content)

# Update lexer.cpp
with open('cpp_frontend/lexer.cpp', 'r') as f:
    cpp_content = f.read()

cpp_content = cpp_content.replace(
'''            else if (ident == "byte") tokens.push_back({TokenType::TOK_BYTE_TYPE, ident, startLine, startCol});''',
'''            else if (ident == "byte") tokens.push_back({TokenType::TOK_BYTE_TYPE, ident, startLine, startCol});
            else if (ident == "float4") tokens.push_back({TokenType::TOK_FLOAT4_TYPE, ident, startLine, startCol});
            else if (ident == "float8") tokens.push_back({TokenType::TOK_FLOAT8_TYPE, ident, startLine, startCol});
            else if (ident == "int4") tokens.push_back({TokenType::TOK_INT4_TYPE, ident, startLine, startCol});
            else if (ident == "int8") tokens.push_back({TokenType::TOK_INT8_TYPE, ident, startLine, startCol});'''
)

with open('cpp_frontend/lexer.cpp', 'w') as f:
    f.write(cpp_content)

print("Lexer updated")
