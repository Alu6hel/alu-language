import re

with open('cpp_frontend/ast.h', 'r') as f:
    content = f.read()

content = content.replace('''return auto n = std::make_unique<VarDeclNode>''', '''auto n = std::make_unique<VarDeclNode>''')

with open('cpp_frontend/ast.h', 'w') as f:
    f.write(content)


with open('cpp_frontend/parser.cpp', 'r') as f:
    content = f.read()

# Fix parser.cpp missing refinement_var declaration
parser_var = '''std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    Token startTok = currentToken();
    std::string typeStr = parseTypeString();
    std::string refinement_var = "";
    std::unique_ptr<ASTNode> refinement_expr = nullptr;
    if (currentToken().type == TokenType::TOK_LBRACE) {'''

# Find the start of parseVarDecl
orig_parse_var_decl = '''std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    Token startTok = currentToken();
    std::string typeStr = parseTypeString();
      std::string refinement_var = "";
      std::unique_ptr<ASTNode> refinement_expr = nullptr;
      if (currentToken().type == TokenType::TOK_LBRACE) {'''

# It's already there, wait! If it's already there, why did it error?
# Let's read parser.cpp lines 295-310 to see what happened.
