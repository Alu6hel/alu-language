#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

enum class DataType {
    UNKNOWN,
    INT,
    STRING,
    BOOL,
    VOID,
    POINTER,
    ARRAY
};

inline std::string DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::STRING: return "STRING";
        case DataType::BOOL: return "BOOL";
        case DataType::VOID: return "VOID";
        case DataType::POINTER: return "POINTER";
        case DataType::ARRAY: return "ARRAY";
        default: return "UNKNOWN";
    }
}

class LLVMCodeGen;

// Base AST Node
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
    virtual void codegen(LLVMCodeGen& cg) = 0;
};

// Inline Assembly Call: asm("...");
class AsmCallNode : public ASTNode {
public:
    std::string instruction;
    AsmCallNode(std::string instr) : instruction(instr) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[AsmCall] -> " << instruction << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
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
    void codegen(LLVMCodeGen& cg) override;
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
    void codegen(LLVMCodeGen& cg) override;
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
    void codegen(LLVMCodeGen& cg) override;
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
    void codegen(LLVMCodeGen& cg) override;
};

class FreeNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    FreeNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Free]" << std::endl;
        expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Variable Assignment Node
class VarAssignNode : public ASTNode {
public:
    std::string name;
    std::unique_ptr<ASTNode> expr;
    VarAssignNode(std::string n, std::unique_ptr<ASTNode> e) : name(n), expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[VarAssign] " << name << " =" << std::endl;
        expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// If Node
class IfNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> then_body;
    std::vector<std::unique_ptr<ASTNode>> else_body;
    IfNode(std::unique_ptr<ASTNode> cond) : condition(std::move(cond)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[If]" << std::endl;
        condition->print(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "[Then]" << std::endl;
        for (const auto& stmt : then_body) stmt->print(indent + 6);
        if (!else_body.empty()) {
            std::cout << std::string(indent + 2, ' ') << "[Else]" << std::endl;
            for (const auto& stmt : else_body) stmt->print(indent + 6);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
};

// While Node
class WhileNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::vector<std::unique_ptr<ASTNode>> body;
    WhileNode(std::unique_ptr<ASTNode> cond) : condition(std::move(cond)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[While]" << std::endl;
        condition->print(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "[Body]" << std::endl;
        for (const auto& stmt : body) stmt->print(indent + 6);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Return Node
class ReturnNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    ReturnNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Return]" << std::endl;
        if (expr) expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Function Call Node
class FuncCallNode : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> args;
    FuncCallNode(std::string n, std::vector<std::unique_ptr<ASTNode>> a) : name(n), args(std::move(a)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[FuncCall] " << name << std::endl;
        for (const auto& a : args) a->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Parameter Struct
struct Parameter {
    std::string type;
    std::string name;
};

// Struct Field Struct
struct StructField {
    std::string type;
    std::string name;
};

// Struct Definition Node
class StructDefNode : public ASTNode {
public:
    std::string name;
    std::vector<StructField> fields;
    StructDefNode(std::string n, std::vector<StructField> f) : name(n), fields(f) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[StructDef] " << name << std::endl;
        for (const auto& field : fields) {
            std::cout << std::string(indent + 4, ' ') << field.type << " " << field.name << std::endl;
        }
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Member Access Node: p.x
class MemberAccessNode : public ASTNode {
public:
    std::string objectName;
    std::string fieldName;
    MemberAccessNode(std::string obj, std::string field) : objectName(obj), fieldName(field) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[MemberAccess] " << objectName << "." << fieldName << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Member Assignment Node: p.x = 5;
class MemberAssignNode : public ASTNode {
public:
    std::string objectName;
    std::string fieldName;
    std::unique_ptr<ASTNode> expr;
    MemberAssignNode(std::string obj, std::string field, std::unique_ptr<ASTNode> e) 
        : objectName(obj), fieldName(field), expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[MemberAssign] " << objectName << "." << fieldName << " =" << std::endl;
        expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Extern Routine Declaration Node
class ExternRoutineNode : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> params;
    bool isVariadic;
    std::string returnType;
    
    ExternRoutineNode(std::string n, std::vector<Parameter> p, bool v, std::string rt) 
        : name(n), params(p), isVariadic(v), returnType(rt) {}
        
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ExternRoutine] " << name << "(";
        for (size_t i = 0; i < params.size(); ++i) {
            std::cout << params[i].type << " " << params[i].name;
            if (i < params.size() - 1) std::cout << ", ";
        }
        if (isVariadic) {
            if (!params.empty()) std::cout << ", ";
            std::cout << "...";
        }
        std::cout << ") -> " << returnType << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Routine Node: routine calculate(int x, int y) -> int { ... }
class RoutineNode : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    std::vector<std::unique_ptr<ASTNode>> body;
    RoutineNode(std::string n) : name(n), returnType("void") {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[RoutineDef] " << name << "(";
        for (size_t i = 0; i < params.size(); ++i) {
            std::cout << params[i].type << " " << params[i].name;
            if (i < params.size() - 1) std::cout << ", ";
        }
        std::cout << ") -> " << returnType << std::endl;
        for (const auto& stmt : body) {
            stmt->print(indent + 4);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Variable Access Node
class VarAccessNode : public ASTNode {
public:
    std::string name;
    VarAccessNode(std::string n) : name(n) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[VarAccess] " << name << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Pointer/Memory Nodes
class AddressOfNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    AddressOfNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[AddressOf]" << std::endl;
        expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

class DereferenceNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    DereferenceNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Dereference]" << std::endl;
        expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

class DerefAssignNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> ptr_expr;
    std::unique_ptr<ASTNode> val_expr;
    DerefAssignNode(std::unique_ptr<ASTNode> p, std::unique_ptr<ASTNode> v) : ptr_expr(std::move(p)), val_expr(std::move(v)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[DerefAssign]" << std::endl;
        ptr_expr->print(indent + 4);
        val_expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
};

class NewAllocationNode : public ASTNode {
public:
    std::string typeName;
    NewAllocationNode(std::string t) : typeName(t) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[New] " << typeName << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

// Array Nodes
class ArrayDeclNode : public ASTNode {
public:
    std::string type;
    std::string name;
    std::unique_ptr<ASTNode> sizeExpr;
    ArrayDeclNode(std::string t, std::string n, std::unique_ptr<ASTNode> s) : type(t), name(n), sizeExpr(std::move(s)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ArrayDecl] " << type << " " << name << "[" << std::endl;
        sizeExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "]" << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

class ArrayIndexNode : public ASTNode {
public:
    std::string name;
    std::unique_ptr<ASTNode> indexExpr;
    ArrayIndexNode(std::string n, std::unique_ptr<ASTNode> idx) : name(n), indexExpr(std::move(idx)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ArrayIndex] " << name << "[" << std::endl;
        indexExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "]" << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
};

class ArrayAssignNode : public ASTNode {
public:
    std::string name;
    std::unique_ptr<ASTNode> indexExpr;
    std::unique_ptr<ASTNode> valExpr;
    ArrayAssignNode(std::string n, std::unique_ptr<ASTNode> idx, std::unique_ptr<ASTNode> v) 
        : name(n), indexExpr(std::move(idx)), valExpr(std::move(v)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ArrayAssign] " << name << "[" << std::endl;
        indexExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "] =" << std::endl;
        valExpr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
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
    void codegen(LLVMCodeGen& cg) override;
};
