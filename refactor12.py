import re

with open('cpp_frontend/parser.cpp', 'r') as f:
    content = f.read()

# Update parseRoutine
routine_param_orig = '''std::string pType = parseTypeString();
        // pointers and arrays are now handled by parseTypeString, so we don't need the inner while loop here
        std::string pName = currentToken().value;
        advance();
        routine->params.push_back({pType, pName});'''

routine_param_new = '''std::string pType = parseTypeString();
        std::string pRefVar = "";
        std::shared_ptr<ASTNode> pRefExpr = nullptr;
        if (currentToken().type == TokenType::TOK_LBRACE) {
            advance(); // {
            pRefVar = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            expect(TokenType::TOK_BIT_OR); // |
            pRefExpr = std::shared_ptr<ASTNode>(parseExpression().release());
            expect(TokenType::TOK_RBRACE); // }
        }
        
        std::string pName = currentToken().value;
        advance();
        routine->params.push_back({pType, pName, pRefVar, pRefExpr});'''

content = content.replace(routine_param_orig, routine_param_new)

with open('cpp_frontend/parser.cpp', 'w') as f:
    f.write(content)

print("Updated parseRoutine")
