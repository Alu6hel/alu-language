import re

with open('cpp_frontend/parser.cpp', 'r') as f:
    content = f.read()

target = '''    } else if (currentToken().type == TokenType::TOK_INT_TYPE || 
               currentToken().type == TokenType::TOK_FLOAT_TYPE || 
               currentToken().type == TokenType::TOK_DOUBLE_TYPE || 
               currentToken().type == TokenType::TOK_BYTE_TYPE) {'''

new_target = '''    } else if (currentToken().type == TokenType::TOK_FLOAT4_TYPE || 
               currentToken().type == TokenType::TOK_FLOAT8_TYPE || 
               currentToken().type == TokenType::TOK_INT4_TYPE || 
               currentToken().type == TokenType::TOK_INT8_TYPE) {
        std::string vtype = currentToken().value;
        advance(); // consume type token
        expect(TokenType::TOK_LPAREN);
        std::vector<std::unique_ptr<ASTNode>> args;
        if (currentToken().type != TokenType::TOK_RPAREN) {
            args.push_back(parseExpression());
            while (currentToken().type == TokenType::TOK_COMMA) {
                advance();
                args.push_back(parseExpression());
            }
        }
        expect(TokenType::TOK_RPAREN);
        left = attachLoc(std::make_unique<VectorInitNode>(vtype, std::move(args)), startTok);
    } else if (currentToken().type == TokenType::TOK_INT_TYPE || 
               currentToken().type == TokenType::TOK_FLOAT_TYPE || 
               currentToken().type == TokenType::TOK_DOUBLE_TYPE || 
               currentToken().type == TokenType::TOK_BYTE_TYPE) {'''

content = content.replace(target, new_target)

with open('cpp_frontend/parser.cpp', 'w') as f:
    f.write(content)

print("Parser updated")
