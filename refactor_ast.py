import re

with open('cpp_frontend/ast.h', 'r') as f:
    ast_content = f.read()

# Find a good place to insert, right before UnaryOpNode or similar
insertion_target = "class UnaryOpNode : public ASTNode {"
new_node = '''class VectorInitNode : public ASTNode {
public:
    std::string typeName;
    std::vector<std::unique_ptr<ASTNode>> elements;

    VectorInitNode(std::string t, std::vector<std::unique_ptr<ASTNode>> elems) 
        : typeName(t), elements(std::move(elems)) {}
        
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[VectorInit] " << typeName << std::endl;
        for (const auto& el : elements) {
            el->print(indent + 4);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<std::unique_ptr<ASTNode>> cloned_elems;
        for (const auto& e : elements) cloned_elems.push_back(e->clone(type_map));
        return std::make_unique<VectorInitNode>(typeName, std::move(cloned_elems));
    }
};

class UnaryOpNode : public ASTNode {'''

ast_content = ast_content.replace(insertion_target, new_node)

with open('cpp_frontend/ast.h', 'w') as f:
    f.write(ast_content)

print("AST updated")
