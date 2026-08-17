import re

with open('cpp_frontend/parser.cpp', 'r') as f:
    content = f.read()

# Update parseVarDecl
var_decl_orig = '''std::string typeStr = parseTypeString();
  
      
      // pointers/arrays are handled in parseTypeString'''

var_decl_new = '''std::string typeStr = parseTypeString();
      std::string refinement_var = "";
      std::unique_ptr<ASTNode> refinement_expr = nullptr;
      if (currentToken().type == TokenType::TOK_LBRACE) {
          advance(); // {
          refinement_var = currentToken().value;
          expect(TokenType::TOK_IDENTIFIER);
          expect(TokenType::TOK_BIT_OR); // |
          refinement_expr = parseExpression();
          expect(TokenType::TOK_RBRACE); // }
      }'''

content = content.replace(var_decl_orig, var_decl_new)

var_decl_ret_orig = '''return attachLoc(std::make_unique<VarDeclNode>(typeStr, name, std::move(expr)), startTok);'''
var_decl_ret_new = '''auto node = std::make_unique<VarDeclNode>(typeStr, name, std::move(expr));
      node->refinement_var = refinement_var;
      node->refinement_expr = std::move(refinement_expr);
      return attachLoc(std::move(node), startTok);'''

content = content.replace(var_decl_ret_orig, var_decl_ret_new)


with open('cpp_frontend/parser.cpp', 'w') as f:
    f.write(content)

print("Updated parseVarDecl")
