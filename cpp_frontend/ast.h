#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

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
