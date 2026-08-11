#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <regex>
#include <iostream>

inline std::string replaceTypeVars(std::string typeStr, const std::map<std::string, std::string>& type_map) {
    if (type_map.count(typeStr)) return type_map.at(typeStr);
    std::string result = typeStr;
    for (const auto& kv : type_map) {
        std::regex re("\\b" + kv.first + "\\b");
        result = std::regex_replace(result, re, kv.second);
    }
    return result;
}

enum class DataType {
    UNKNOWN,
    INT,
    STRING,
    BOOL,
    VOID,
    POINTER,
    ARRAY,
    FLOAT,
    DOUBLE,
    BYTE
};

inline std::string DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::STRING: return "STRING";
        case DataType::BOOL: return "BOOL";
        case DataType::VOID: return "VOID";
        case DataType::POINTER: return "POINTER";
        case DataType::ARRAY: return "ARRAY";
        case DataType::FLOAT: return "FLOAT";
        case DataType::DOUBLE: return "DOUBLE";
        case DataType::BYTE: return "BYTE";
        default: return "UNKNOWN";
    }
}

class LLVMCodeGen;

// Base AST Node
class ASTNode {
public:
    int line = 0;
    int col = 0;
    std::string file = "";
    
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
    virtual void codegen(LLVMCodeGen& cg) = 0;
    virtual std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const = 0;
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<AsmCallNode>(instruction);
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
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<UnsafeBlockNode>(); for (const auto& s : body) n->body.push_back(s->clone(type_map)); return n;
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
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<LiteralNode>(type, value);
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
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<BinOpNode>(op, left->clone(type_map), right->clone(type_map));
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
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<VarDeclNode>(replaceTypeVars(varType, type_map), name, initializer ? initializer->clone(type_map) : nullptr);
    }
};

class ThrowNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    
    ThrowNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Throw]\n";
        expr->print(indent + 2);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ThrowNode>(expr->clone(type_map));
    }
};

class TryCatchNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> try_body;
    std::string catch_var_type;
    std::string catch_var_name;
    std::vector<std::unique_ptr<ASTNode>> catch_body;
    
    TryCatchNode(std::vector<std::unique_ptr<ASTNode>> t_body,
                 std::string c_type,
                 std::string c_name,
                 std::vector<std::unique_ptr<ASTNode>> c_body)
        : try_body(std::move(t_body)), catch_var_type(c_type), catch_var_name(c_name), catch_body(std::move(c_body)) {}
        
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Try]\n";
        for (const auto& stmt : try_body) stmt->print(indent + 2);
        std::cout << std::string(indent, ' ') << "[Catch] " << catch_var_type << " " << catch_var_name << "\n";
        for (const auto& stmt : catch_body) stmt->print(indent + 2);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<std::unique_ptr<ASTNode>> t_body; for(auto& s: try_body) t_body.push_back(s->clone(type_map)); std::vector<std::unique_ptr<ASTNode>> c_body; for(auto& s: catch_body) c_body.push_back(s->clone(type_map)); return std::make_unique<TryCatchNode>(std::move(t_body), replaceTypeVars(catch_var_type, type_map), catch_var_name, std::move(c_body));
    }
};

class CastNode : public ASTNode {
public:
    DataType targetType;
    std::unique_ptr<ASTNode> expr;
    
    CastNode(DataType t, std::unique_ptr<ASTNode> e) : targetType(t), expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::string ts = (targetType == DataType::INT) ? "int" : (targetType == DataType::FLOAT) ? "float" : "byte";
        std::cout << std::string(indent, ' ') << "[Cast] to " << ts << "\n";
        expr->print(indent + 2);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<CastNode>(targetType, expr->clone(type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<FreeNode>(expr->clone(type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<VarAssignNode>(name, expr->clone(type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<IfNode>(condition->clone(type_map)); for(auto& s: then_body) n->then_body.push_back(s->clone(type_map)); for(auto& s: else_body) n->else_body.push_back(s->clone(type_map)); return n;
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<WhileNode>(condition->clone(type_map)); for(auto& s: body) n->body.push_back(s->clone(type_map)); return n;
    }
};

// For Node: for (init; cond; update) { body }
class ForNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> update;
    std::vector<std::unique_ptr<ASTNode>> body;
    ForNode(std::unique_ptr<ASTNode> i, std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> u)
        : init(std::move(i)), condition(std::move(c)), update(std::move(u)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[For]" << std::endl;
        if (init) { std::cout << std::string(indent + 2, ' ') << "[Init]" << std::endl; init->print(indent + 6); }
        if (condition) { std::cout << std::string(indent + 2, ' ') << "[Cond]" << std::endl; condition->print(indent + 6); }
        if (update) { std::cout << std::string(indent + 2, ' ') << "[Update]" << std::endl; update->print(indent + 6); }
        std::cout << std::string(indent + 2, ' ') << "[Body]" << std::endl;
        for (const auto& stmt : body) stmt->print(indent + 6);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<ForNode>(init ? init->clone(type_map) : nullptr, condition ? condition->clone(type_map) : nullptr, update ? update->clone(type_map) : nullptr); for(auto& s: body) n->body.push_back(s->clone(type_map)); return n;
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ReturnNode>(expr ? expr->clone(type_map) : nullptr);
    }
};

// Assert Node
class AssertNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    AssertNode(std::unique_ptr<ASTNode> cond) : condition(std::move(cond)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Assert]" << std::endl;
        if (condition) condition->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<AssertNode>(condition ? condition->clone(type_map) : nullptr);
    }
};

// Effect Declaration Node
class EffectDeclNode : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> methods; // RoutineNode representing signatures
    EffectDeclNode(std::string n) : name(n) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[EffectDef] " << name << std::endl;
        for (const auto& m : methods) m->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<EffectDeclNode>(name); for(auto& s: methods) n->methods.push_back(s->clone(type_map)); return n;
    }
};

// Yield Node
class YieldNode : public ASTNode {
public:
    std::string effect_name;
    std::string method_name;
    std::vector<std::string> type_args;
    std::vector<std::unique_ptr<ASTNode>> args;
    YieldNode(std::string en, std::string mn) : effect_name(en), method_name(mn) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Yield] " << effect_name << "." << method_name << std::endl;
        for (const auto& a : args) a->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<YieldNode>(effect_name, method_name); for(auto& a: args) n->args.push_back(a->clone(type_map)); return n;
    }
};

// Resume Node
class ResumeNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    ResumeNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Resume]" << std::endl;
        if (expr) expr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ResumeNode>(expr ? expr->clone(type_map) : nullptr);
    }
};

// Handle Node
class HandleNode : public ASTNode {
public:
    std::string effect_name;
    std::string handler_method;
    std::vector<std::pair<DataType, std::string>> handler_args;
    std::vector<std::unique_ptr<ASTNode>> handler_body; // 'on' block
    std::unique_ptr<ASTNode> in_call; // 'in' block function call
    
    HandleNode(std::string en) : effect_name(en) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Handle] " << effect_name << std::endl;
        std::cout << std::string(indent + 2, ' ') << "[On] " << handler_method << std::endl;
        for (const auto& stmt : handler_body) stmt->print(indent + 6);
        std::cout << std::string(indent + 2, ' ') << "[In]" << std::endl;
        if (in_call) in_call->print(indent + 6);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<HandleNode>(effect_name); n->handler_method = handler_method; n->handler_args = handler_args; for(auto& s: handler_body) n->handler_body.push_back(s->clone(type_map)); if(in_call) n->in_call = in_call->clone(type_map); return n;
    }
};

// Function Call Node
class FuncCallNode : public ASTNode {
public:
    std::string name;
    std::vector<std::string> type_args;
    std::vector<std::unique_ptr<ASTNode>> args;
    FuncCallNode(std::string n, std::vector<std::unique_ptr<ASTNode>> a) : name(n), args(std::move(a)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[FuncCall] " << name << std::endl;
        for (const auto& a : args) a->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<std::unique_ptr<ASTNode>> a; for(auto& x: args) a.push_back(x->clone(type_map)); auto n = std::make_unique<FuncCallNode>(name, std::move(a)); n->type_args = type_args; return n;
    }
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
    std::vector<std::string> type_params;
    std::vector<StructField> fields;
    std::vector<std::unique_ptr<ASTNode>> requires_annotations;
    std::vector<std::unique_ptr<ASTNode>> ensures_annotations;
    bool isExported = false;
    StructDefNode(std::string n, std::vector<std::string> tp, std::vector<StructField> f, bool exported = false) : name(n), type_params(tp), fields(f), isExported(exported) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[StructDef] " << name;
        if (isExported) {
            std::cout << " (exported)";
        }
        if (!type_params.empty()) {
            std::cout << "<";
            for (size_t i = 0; i < type_params.size(); ++i) {
                std::cout << type_params[i] << (i < type_params.size() - 1 ? ", " : "");
            }
            std::cout << ">";
        }
        std::cout << std::endl;
        for (const auto& req : requires_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@requires]" << std::endl;
            req->print(indent + 8);
        }
        for (const auto& ens : ensures_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@ensures]" << std::endl;
            ens->print(indent + 8);
        }
        for (const auto& field : fields) {
            std::cout << std::string(indent + 4, ' ') << field.type << " " << field.name << ";" << std::endl;
        }
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<StructField> f; for(auto& x: fields) f.push_back({replaceTypeVars(x.type, type_map), x.name}); auto n = std::make_unique<StructDefNode>(name, type_params, f, isExported); for(auto& x: requires_annotations) n->requires_annotations.push_back(x->clone(type_map)); for(auto& x: ensures_annotations) n->ensures_annotations.push_back(x->clone(type_map)); return n;
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<MemberAccessNode>(objectName, fieldName);
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<MemberAssignNode>(objectName, fieldName, expr->clone(type_map));
    }
};

// Extern Routine Declaration Node
class ExternRoutineNode : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> params;
    bool isVariadic;
    std::string returnType;
    std::vector<std::unique_ptr<ASTNode>> requires_annotations;
    std::vector<std::unique_ptr<ASTNode>> ensures_annotations;
    
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
        for (const auto& req : requires_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@requires]" << std::endl;
            req->print(indent + 8);
        }
        for (const auto& ens : ensures_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@ensures]" << std::endl;
            ens->print(indent + 8);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<Parameter> p; for(auto& x: params) p.push_back({replaceTypeVars(x.type, type_map), x.name}); auto n = std::make_unique<ExternRoutineNode>(name, p, isVariadic, replaceTypeVars(returnType, type_map)); for(auto& x: requires_annotations) n->requires_annotations.push_back(x->clone(type_map)); for(auto& x: ensures_annotations) n->ensures_annotations.push_back(x->clone(type_map)); return n;
    }
};

struct Receiver {
    std::string name;
    std::string type;
    bool isPointer;
};

// Routine Node: routine calculate(int x, int y) -> int { ... }
class RoutineNode : public ASTNode {
public:
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    std::vector<std::string> type_params;
    std::vector<std::unique_ptr<ASTNode>> body;
    std::optional<Receiver> receiver;
    bool isExported = false;
    std::vector<std::unique_ptr<ASTNode>> requires_annotations;
    std::vector<std::unique_ptr<ASTNode>> ensures_annotations;

    RoutineNode(std::string n, std::optional<Receiver> rec = std::nullopt, bool exported = false) : name(n), returnType("void"), receiver(rec), isExported(exported) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[RoutineDef] ";
        if (isExported) {
            std::cout << "(exported) ";
        }
        if (receiver) {
            std::cout << "(" << receiver->name << ": " << receiver->type << (receiver->isPointer ? "*" : "") << ") ";
        }
        std::cout << name << "(";
        for (size_t i = 0; i < params.size(); ++i) {
            std::cout << params[i].type << " " << params[i].name;
            if (i < params.size() - 1) std::cout << ", ";
        }
        std::cout << ") -> " << returnType << std::endl;
        for (const auto& req : requires_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@requires]" << std::endl;
            req->print(indent + 8);
        }
        for (const auto& ens : ensures_annotations) {
            std::cout << std::string(indent + 4, ' ') << "[@ensures]" << std::endl;
            ens->print(indent + 8);
        }
        for (const auto& stmt : body) {
            stmt->print(indent + 4);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::optional<Receiver> rec = receiver; if (rec) { rec->type = replaceTypeVars(rec->type, type_map); } auto n = std::make_unique<RoutineNode>(name, rec, isExported); for(auto& x: params) n->params.push_back({replaceTypeVars(x.type, type_map), x.name}); n->returnType = replaceTypeVars(returnType, type_map); for(auto& s: body) n->body.push_back(s->clone(type_map)); for(auto& x: requires_annotations) n->requires_annotations.push_back(x->clone(type_map)); for(auto& x: ensures_annotations) n->ensures_annotations.push_back(x->clone(type_map)); n->type_params = type_params; return n;
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<VarAccessNode>(name);
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<AddressOfNode>(expr->clone(type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<DereferenceNode>(expr->clone(type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<DerefAssignNode>(ptr_expr->clone(type_map), val_expr->clone(type_map));
    }
};

class NewAllocationNode : public ASTNode {
public:
    std::string typeName;
    NewAllocationNode(std::string t) : typeName(t) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[New] " << typeName << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<NewAllocationNode>(replaceTypeVars(typeName, type_map));
    }
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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ArrayDeclNode>(replaceTypeVars(type, type_map), name, sizeExpr->clone(type_map));
    }
};

class ArrayIndexNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> arrayExpr;
    std::unique_ptr<ASTNode> indexExpr;
    ArrayIndexNode(std::unique_ptr<ASTNode> arr, std::unique_ptr<ASTNode> idx) : arrayExpr(std::move(arr)), indexExpr(std::move(idx)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ArrayIndex]" << std::endl;
        arrayExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "[" << std::endl;
        indexExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "]" << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ArrayIndexNode>(arrayExpr->clone(type_map), indexExpr->clone(type_map));
    }
};

class ArrayAssignNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> arrayExpr;
    std::unique_ptr<ASTNode> indexExpr;
    std::unique_ptr<ASTNode> valExpr;
    ArrayAssignNode(std::unique_ptr<ASTNode> arr, std::unique_ptr<ASTNode> idx, std::unique_ptr<ASTNode> v) 
        : arrayExpr(std::move(arr)), indexExpr(std::move(idx)), valExpr(std::move(v)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[ArrayAssign]" << std::endl;
        arrayExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "[" << std::endl;
        indexExpr->print(indent + 4);
        std::cout << std::string(indent, ' ') << "] =" << std::endl;
        valExpr->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ArrayAssignNode>(arrayExpr->clone(type_map), indexExpr->clone(type_map), valExpr->clone(type_map));
    }
};

// Program Root Node
// Method Call Node
class MethodCallNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> object;
    std::string methodName;
    std::vector<std::string> type_args;
    std::vector<std::unique_ptr<ASTNode>> args;
    MethodCallNode(std::unique_ptr<ASTNode> obj, std::string name, std::vector<std::unique_ptr<ASTNode>> a) 
        : object(std::move(obj)), methodName(name), args(std::move(a)) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[MethodCall] ." << methodName << std::endl;
        if (object) object->print(indent + 4);
        for (const auto& a : args) a->print(indent + 4);
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        std::vector<std::unique_ptr<ASTNode>> a; for(auto& x: args) a.push_back(x->clone(type_map)); return std::make_unique<MethodCallNode>(object ? object->clone(type_map) : nullptr, methodName, std::move(a));
    }
};

// Import Node
class ImportNode : public ASTNode {
public:
    std::string moduleName;
    bool isModulePath = false; // true for `import std::fs;`, false for `import "file.alu";`
    std::string alias; // alias for namespace wrapping, e.g. `import "file.alu" as f;`
    ImportNode(std::string name, bool modPath = false, std::string aliasName = "") 
        : moduleName(name), isModulePath(modPath), alias(aliasName) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Import] " << moduleName
                  << (isModulePath ? " (module)" : " (file)") 
                  << (alias.empty() ? "" : " as " + alias) << std::endl;
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        return std::make_unique<ImportNode>(moduleName, isModulePath, alias);
    }
};

// Namespace Node
class NamespaceNode : public ASTNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> declarations;
    NamespaceNode(std::string name) : name(name) {}
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[Namespace] " << name << std::endl;
        for (const auto& decl : declarations) {
            decl->print(indent + 4);
        }
    }
    void codegen(LLVMCodeGen& cg) override;
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<NamespaceNode>(name); for(auto& s: declarations) n->declarations.push_back(s->clone(type_map)); return n;
    }
};

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
    std::unique_ptr<ASTNode> clone(const std::map<std::string, std::string>& type_map) const override {
        auto n = std::make_unique<ProgramNode>(); for(auto& s: declarations) n->declarations.push_back(s->clone(type_map)); return n;
    }
};
