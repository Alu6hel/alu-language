import re

with open('cpp_frontend/ast.h', 'r') as f:
    content = f.read()

# Update VarDeclNode
var_decl_orig = '''class VarDeclNode : public ASTNode {
public:
    std::string varType;
    std::string name;
    std::unique_ptr<ASTNode> initializer;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<ASTNode> init) 
        : varType(t), name(n), initializer(std::move(init)) {}'''

var_decl_new = '''class VarDeclNode : public ASTNode {
public:
    std::string varType;
    std::string refinement_var;
    std::unique_ptr<ASTNode> refinement_expr;
    std::string name;
    std::unique_ptr<ASTNode> initializer;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<ASTNode> init) 
        : varType(t), refinement_var(""), refinement_expr(nullptr), name(n), initializer(std::move(init)) {}'''

content = content.replace(var_decl_orig, var_decl_new)

clone_orig = '''std::make_unique<VarDeclNode>(replaceTypeVars(varType, type_map), name, initializer ? initializer->clone(type_map) : nullptr);'''
clone_new = '''auto n = std::make_unique<VarDeclNode>(replaceTypeVars(varType, type_map), name, initializer ? initializer->clone(type_map) : nullptr);
        n->refinement_var = refinement_var;
        if (refinement_expr) n->refinement_expr = refinement_expr->clone(type_map);
        return n;'''

content = content.replace(clone_orig, clone_new)

with open('cpp_frontend/ast.h', 'w') as f:
    f.write(content)

print("Updated VarDeclNode in ast.h")
