#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

enum class DataType {
    UNKNOWN,
    INT,
    STRING,
    VOID
};

inline std::string DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::STRING: return "STRING";
        case DataType::VOID: return "VOID";
        default: return "UNKNOWN";
    }
}

// Base AST Node
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
};

// Inline Assembly Call: asm("...");
class AsmCallNode : public ASTNode {
public:
    std::string instruction;
    AsmCallNode(std::string instr) : instruction(instr) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[AsmCall] -> " << instruction << std::endl;
    }
};

// Unsafe Block: unsafe { ... }
class UnsafeBlockNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> body;
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[UnsafeBlock]" << std::endl;
        for (const auto& stmt : body) {
            stmt->print(indent + 4);
        }
    }
};

// Literal Node (e.g., 5 or "hello")
class LiteralNode : public ASTNode {
public:
    DataType type;
    std::string value;
    LiteralNode(DataType t, std::string v) : type(t), value(v) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Literal] " << value << std::endl;
    }
};

// Binary Operation Node (e.g., +)
class BinOpNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    BinOpNode(std::string o, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[BinOp] " << op << std::endl;
        left->print(indent + 4);
        right->print(indent + 4);
    }
};

// Variable Declaration Node: int x = 5;
class VarDeclNode : public ASTNode {
public:
    std::string varType;
    std::string name;
    std::unique_ptr<ASTNode> initializer;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<ASTNode> init) 
        : varType(t), name(n), initializer(std::move(init)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[VarDecl] " << varType << " " << name << std::endl;
        if (initializer) initializer->print(indent + 4);
    }
};

// Routine Node: routine name() { ... }
class RoutineNode : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> body;
    RoutineNode(std::string n) : name(n) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[RoutineDef] " << name << "()" << std::endl;
        for (const auto& stmt : body) {
            stmt->print(indent + 4);
        }
    }
};

// Program Root Node
class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> declarations;
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ProgramRoot]" << std::endl;
        for (const auto& decl : declarations) {
            decl->print(indent + 4);
        }
    }
};
