#include "llvm_codegen.h"
#include <fstream>
#include <iostream>

LLVMCodeGen::LLVMCodeGen() : tmp_counter(1) {}

std::string LLVMCodeGen::getIR() const {
    return ir_output.str();
}

void LLVMCodeGen::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (out.is_open()) {
        out << ir_output.str();
        out.close();
    }
}

std::string LLVMCodeGen::getTempReg() {
    return "%t" + std::to_string(tmp_counter++);
}

std::string LLVMCodeGen::getLabel(const std::string& prefix) {
    return prefix + std::to_string(label_counter++);
}

void LLVMCodeGen::emit(const std::string& code) {
    ir_output << code << "\n";
}

// AST Node accept methods (Double Dispatch)
void AsmCallNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void UnsafeBlockNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void LiteralNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void BinOpNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void VarDeclNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void VarAssignNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void IfNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void WhileNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ReturnNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void FuncCallNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void RoutineNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ExternRoutineNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void StructDefNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void MemberAccessNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void MemberAssignNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void VarAccessNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void AddressOfNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void DereferenceNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void DerefAssignNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void FreeNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void NewAllocationNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ArrayDeclNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ArrayIndexNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ArrayAssignNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ProgramNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
// --- LLVMCodeGen Visitor Implementation --- //

std::string LLVMCodeGen::evaluateExpression(ASTNode* expr) {
    if (!expr) return "";
    if (auto lit = dynamic_cast<LiteralNode*>(expr)) return visit(lit);
    if (auto binop = dynamic_cast<BinOpNode*>(expr)) return visit(binop);
    if (auto func = dynamic_cast<FuncCallNode*>(expr)) return visit(func);
    if (auto maccess = dynamic_cast<MemberAccessNode*>(expr)) return visit(maccess);
    if (auto vacc = dynamic_cast<VarAccessNode*>(expr)) return visit(vacc);
    if (auto addr = dynamic_cast<AddressOfNode*>(expr)) return visit(addr);
    if (auto deref = dynamic_cast<DereferenceNode*>(expr)) return visit(deref);
    if (auto alloc = dynamic_cast<NewAllocationNode*>(expr)) return visit(alloc);
    if (auto arrIdx = dynamic_cast<ArrayIndexNode*>(expr)) return visit(arrIdx);
    return "";
}

void LLVMCodeGen::visit(AsmCallNode* node) {
    // Map our custom asm("Hello World") directly to a C-standard puts() call in LLVM IR
    std::string text = node->instruction;
    // Strip quotes for LLVM IR string format
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }
    
    // Allocate global string constant (simplified for prototype, technically should be declared globally)
    // We'll just use a hack: allocate it on stack and pass to puts.
    // Actually, in LLVM IR, passing a raw c"..." to a function call works if casted properly, 
    // but the easiest is just a global string. Since we are doing text emission, we can emit the call directly:
    // call i32 @puts(i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str, i64 0, i64 0))
    // We will simplify and just assume it's stored in a variable or emit a basic global.
    
    // The easiest way for text emission prototyping: declare the global string inline as a constant 
    std::string str_name = "@.str." + std::to_string(tmp_counter++);
    std::string str_val = "c\"" + text + "\\00\"";
    int len = text.length() + 1;
    
    // Hack for text IR: we can't emit globals inside a function, so we'll just emit an alloca, store, and call.
    std::string reg = getTempReg();
    emit("  " + reg + " = alloca [" + std::to_string(len) + " x i8], align 1");
    emit("  store [" + std::to_string(len) + " x i8] " + str_val + ", [" + std::to_string(len) + " x i8]* " + reg + ", align 1");
    std::string ptr_reg = getTempReg();
    emit("  " + ptr_reg + " = bitcast [" + std::to_string(len) + " x i8]* " + reg + " to i8*");
    emit("  call i32 @puts(i8* " + ptr_reg + ")");
}

void LLVMCodeGen::visit(UnsafeBlockNode* node) {
    emit("  ; Begin unsafe block");
    for (const auto& stmt : node->body) {
        stmt->codegen(*this);
    }
    emit("  ; End unsafe block");
}
static std::string getLLVMType(const std::string& typeStr) {
    std::string base = typeStr;
    std::string ptr = "";
    if (!base.empty() && base.back() == '*') {
        ptr = "*";
        base.pop_back();
    }
    if (base == "int") return "i32" + ptr;
    if (base == "string") return "i8*" + ptr;
    if (base == "bool") return "i1" + ptr;
    if (base == "void") return "void";
    return "%" + base + ptr;
}

std::string LLVMCodeGen::visit(LiteralNode* node) {
    if (node->type == DataType::INT) {
        return node->value;
    } else if (node->type == DataType::STRING) {
        return "c\"" + node->value + "\\00\""; 
    } else if (node->type == DataType::UNKNOWN) {
        // It's a variable access, we need to load it. For now, assume it's i32.
        std::string reg = getTempReg();
        emit("  " + reg + " = load i32, i32* %" + node->value + ", align 4");
        return reg;
    }
    return "0";
}

std::string LLVMCodeGen::visit(VarAccessNode* node) {
    std::string llvm_type = llvm_var_types[node->name];
    if (llvm_type == "") llvm_type = "i32"; // Fallback
    std::string reg = getTempReg();
    emit("  " + reg + " = load " + llvm_type + ", " + llvm_type + "* %" + node->name + ", align 4");
    return reg;
}

std::string LLVMCodeGen::visit(AddressOfNode* node) {
    // AddressOf only makes sense for variables or fields.
    // For local vars, we just return the alloca name as a value (no load).
    if (auto varNode = dynamic_cast<VarAccessNode*>(node->expr.get())) {
        return "%" + varNode->name;
    }
    return "0"; // Advanced address-of not fully implemented
}

std::string LLVMCodeGen::visit(DereferenceNode* node) {
    std::string ptr_reg = evaluateExpression(node->expr.get());
    // Assume i32* for simple prototype
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load i32, i32* " + ptr_reg + ", align 4");
    return val_reg;
}

void LLVMCodeGen::visit(DerefAssignNode* node) {
    std::string ptr_reg = evaluateExpression(node->ptr_expr.get());
    std::string val_reg = evaluateExpression(node->val_expr.get());
    emit("  store i32 " + val_reg + ", i32* " + ptr_reg + ", align 4");
}

std::string LLVMCodeGen::visit(NewAllocationNode* node) {
    // Call malloc. Malloc takes size in bytes (e.g. 4 for int, or struct size).
    // For simplicity, we allocate 16 bytes for anything.
    std::string reg = getTempReg();
    emit("  " + reg + " = call i8* @malloc(i64 16)");
    std::string cast_reg = getTempReg();
    std::string llvm_type = getLLVMType(node->typeName);
    emit("  " + cast_reg + " = bitcast i8* " + reg + " to " + llvm_type + "*");
    return cast_reg;
}

void LLVMCodeGen::visit(FreeNode* node) {
    std::string ptr_reg = evaluateExpression(node->expr.get());
    std::string cast_reg = getTempReg();
    emit("  " + cast_reg + " = bitcast i32* " + ptr_reg + " to i8*"); // Assuming i32* for now
    emit("  call void @free(i8* " + cast_reg + ")");
}

void LLVMCodeGen::visit(ArrayDeclNode* node) {
    std::string size_val = evaluateExpression(node->sizeExpr.get());
    std::string llvm_type = getLLVMType(node->type);
    
    // alloca array type
    std::string array_llvm_type = "[" + size_val + " x " + llvm_type + "]";
    emit("  %" + node->name + " = alloca " + array_llvm_type + ", align 4");
    llvm_var_types[node->name] = array_llvm_type;
}

std::string LLVMCodeGen::visit(ArrayIndexNode* node) {
    std::string idx_reg = evaluateExpression(node->indexExpr.get());
    std::string arr_type = llvm_var_types[node->name];
    std::string elem_type = "i32"; // Hack: infer from arr_type later
    std::string ptr_reg = getTempReg();
    
    if (arr_type.find("[") != std::string::npos) {
        emit("  " + ptr_reg + " = getelementptr " + arr_type + ", " + arr_type + "* %" + node->name + ", i32 0, i32 " + idx_reg);
    } else {
        // It's a pointer
        emit("  " + ptr_reg + " = getelementptr " + elem_type + ", " + elem_type + "* %" + node->name + ", i32 " + idx_reg);
    }
    
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load " + elem_type + ", " + elem_type + "* " + ptr_reg + ", align 4");
    return val_reg;
}

void LLVMCodeGen::visit(ArrayAssignNode* node) {
    std::string idx_reg = evaluateExpression(node->indexExpr.get());
    std::string val_reg = evaluateExpression(node->valExpr.get());
    std::string arr_type = llvm_var_types[node->name];
    std::string elem_type = "i32"; // Hack
    std::string ptr_reg = getTempReg();
    
    if (arr_type.find("[") != std::string::npos) {
        emit("  " + ptr_reg + " = getelementptr " + arr_type + ", " + arr_type + "* %" + node->name + ", i32 0, i32 " + idx_reg);
    } else {
        // It's a pointer
        emit("  " + ptr_reg + " = getelementptr " + elem_type + ", " + elem_type + "* %" + node->name + ", i32 " + idx_reg);
    }
    
    emit("  store " + elem_type + " " + val_reg + ", " + elem_type + "* " + ptr_reg + ", align 4");
}

std::string LLVMCodeGen::visit(BinOpNode* node) {
    std::string lval = evaluateExpression(node->left.get());
    std::string rval = evaluateExpression(node->right.get());

    std::string res_reg = getTempReg();
    if (node->op == "+") {
        emit("  " + res_reg + " = add i32 " + lval + ", " + rval);
    } else if (node->op == "==") {
        emit("  " + res_reg + " = icmp eq i32 " + lval + ", " + rval);
    } else if (node->op == "<") {
        emit("  " + res_reg + " = icmp slt i32 " + lval + ", " + rval);
    } else if (node->op == ">") {
        emit("  " + res_reg + " = icmp sgt i32 " + lval + ", " + rval);
    }
    return res_reg;
}

void LLVMCodeGen::visit(VarDeclNode* node) {
    // 1. Allocate memory on the stack (alloca)
    std::string llvm_type = getLLVMType(node->varType);
    emit("  %" + node->name + " = alloca " + llvm_type + ", align 4");
    llvm_var_types[node->name] = llvm_type;
    
    if (node->varType != "int" && node->varType != "string") {
        var_struct_type[node->name] = node->varType;
    }
    
    if (node->initializer) {
        std::string init_val = evaluateExpression(node->initializer.get());

        // 2. Store the value into the allocated pointer
        // emit: store i32 %1, i32* %x
        if (init_val != "") {
            emit("  store " + llvm_type + " " + init_val + ", " + llvm_type + "* %" + node->name + ", align 4");
        }
    }
}

void LLVMCodeGen::visit(VarAssignNode* node) {
    std::string val = evaluateExpression(node->expr.get());
    std::string llvm_type = llvm_var_types[node->name];
    if (llvm_type == "") llvm_type = "i32"; // Fallback
    emit("  store " + llvm_type + " " + val + ", " + llvm_type + "* %" + node->name + ", align 4");
}

void LLVMCodeGen::visit(IfNode* node) {
    std::string cond_reg = evaluateExpression(node->condition.get());
    
    std::string then_label = getLabel("if.then");
    std::string else_label = node->else_body.empty() ? getLabel("if.end") : getLabel("if.else");
    std::string end_label = node->else_body.empty() ? else_label : getLabel("if.end");
    
    emit("  br i1 " + cond_reg + ", label %" + then_label + ", label %" + else_label);
    
    emit(then_label + ":");
    for (const auto& stmt : node->then_body) stmt->codegen(*this);
    emit("  br label %" + end_label);
    
    if (!node->else_body.empty()) {
        emit(else_label + ":");
        for (const auto& stmt : node->else_body) stmt->codegen(*this);
        emit("  br label %" + end_label);
    }
    
    emit(end_label + ":");
}

void LLVMCodeGen::visit(WhileNode* node) {
    std::string cond_label = getLabel("while.cond");
    std::string body_label = getLabel("while.body");
    std::string end_label = getLabel("while.end");
    
    emit("  br label %" + cond_label);
    emit(cond_label + ":");
    
    std::string cond_reg = evaluateExpression(node->condition.get());
    
    emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);
    
    emit(body_label + ":");
    for (const auto& stmt : node->body) stmt->codegen(*this);
    emit("  br label %" + cond_label);
    
    emit(end_label + ":");
}

void LLVMCodeGen::visit(ReturnNode* node) {
    if (node->expr) {
        std::string val = evaluateExpression(node->expr.get());
        // Assume i32 for simplicity
        emit("  ret i32 " + val);
    } else {
        emit("  ret void");
    }
}

std::string LLVMCodeGen::visit(FuncCallNode* node) {
    std::vector<std::string> arg_vals;
    std::vector<std::string> arg_types;
    
    for (const auto& arg : node->args) {
        if (auto lit = dynamic_cast<LiteralNode*>(arg.get())) {
            if (lit->type == DataType::STRING) {
                // We need to inline allocate the string for LLVM IR
                std::string text = lit->value;
                
                // Replace \n with \0A for LLVM IR string literal
                std::string processed_text = "";
                for (size_t k = 0; k < text.length(); ++k) {
                    if (text[k] == '\\' && k + 1 < text.length() && text[k+1] == 'n') {
                        processed_text += "\\0A";
                        k++;
                    } else {
                        processed_text += text[k];
                    }
                }
                
                std::string str_val = "c\"" + processed_text + "\\00\"";
                int byte_len = 1; // for \0
                for (size_t k = 0; k < text.length(); ++k) {
                    if (text[k] == '\\' && k + 1 < text.length() && text[k+1] == 'n') {
                        byte_len++;
                        k++;
                    } else {
                        byte_len++;
                    }
                }
                
                std::string reg = getTempReg();
                emit("  " + reg + " = alloca [" + std::to_string(byte_len) + " x i8], align 1");
                emit("  store [" + std::to_string(byte_len) + " x i8] " + str_val + ", [" + std::to_string(byte_len) + " x i8]* " + reg + ", align 1");
                std::string ptr_reg = getTempReg();
                emit("  " + ptr_reg + " = bitcast [" + std::to_string(byte_len) + " x i8]* " + reg + " to i8*");
                
                arg_vals.push_back(ptr_reg);
                arg_types.push_back("i8*");
                continue;
            }
        }
        
        arg_vals.push_back(evaluateExpression(arg.get()));
        arg_types.push_back("i32"); // Assume variables/ints are i32
    }
    
    std::string args_str = "";
    for (size_t i = 0; i < arg_vals.size(); ++i) {
        args_str += arg_types[i] + " " + arg_vals[i];
        if (i < arg_vals.size() - 1) args_str += ", ";
    }
    
    std::string res_reg = getTempReg();
    emit("  " + res_reg + " = call i32 @" + node->name + "(" + args_str + ")");
    return res_reg;
}

void LLVMCodeGen::visit(RoutineNode* node) {
    std::string ret_type = (node->returnType == "int") ? "i32" : "void";
    if (node->name == "main") ret_type = "i32";
    
    std::string params_str = "";
    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string ptype = (node->params[i].type == "int") ? "i32" : "i8*";
        params_str += ptype + " %in_" + node->params[i].name;
        if (i < node->params.size() - 1) params_str += ", ";
    }

    emit("define " + ret_type + " @" + node->name + "(" + params_str + ") {");
    emit("entry:");
    
    // Allocate stack space for parameters
    for (const auto& p : node->params) {
        std::string ptype = (p.type == "int") ? "i32" : "i8*";
        emit("  %" + p.name + " = alloca " + ptype + ", align 4");
        emit("  store " + ptype + " %in_" + p.name + ", " + ptype + "* %" + p.name + ", align 4");
    }
    
    for (const auto& stmt : node->body) {
        stmt->codegen(*this);
    }
    
    // Default return if none provided
    if (ret_type == "i32") {
        emit("  ret i32 0");
    } else {
        emit("  ret void");
    }
    emit("}\n");
}

void LLVMCodeGen::visit(ExternRoutineNode* node) {
    std::string ret_type = (node->returnType == "int") ? "i32" : "void";
    
    std::string params_str = "";
    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string ptype = (node->params[i].type == "int") ? "i32" : "i8*";
        params_str += ptype;
        if (i < node->params.size() - 1 || node->isVariadic) params_str += ", ";
    }
    if (node->isVariadic) {
        params_str += "...";
    }

    emit("declare " + ret_type + " @" + node->name + "(" + params_str + ")\n");
}
void LLVMCodeGen::visit(StructDefNode* node) {
    std::string fields = "";
    for (size_t i = 0; i < node->fields.size(); ++i) {
        std::string lltype = (node->fields[i].type == "int") ? "i32" : ((node->fields[i].type == "string") ? "i8*" : "%" + node->fields[i].type);
        fields += lltype;
        if (i < node->fields.size() - 1) fields += ", ";
        
        struct_field_types[node->name + "." + node->fields[i].name] = lltype;
        struct_field_indices[node->name + "." + node->fields[i].name] = i;
    }
    emit("%" + node->name + " = type { " + fields + " }\n");
}

std::string LLVMCodeGen::visit(MemberAccessNode* node) {
    std::string structName = var_struct_type[node->objectName];
    int fieldIdx = struct_field_indices[structName + "." + node->fieldName];
    std::string fieldType = struct_field_types[structName + "." + node->fieldName];
    
    std::string ptr_reg = getTempReg();
    emit("  " + ptr_reg + " = getelementptr %" + structName + ", %" + structName + "* %" + node->objectName + ", i32 0, i32 " + std::to_string(fieldIdx));
    
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load " + fieldType + ", " + fieldType + "* " + ptr_reg + ", align 4");
    
    return val_reg;
}

void LLVMCodeGen::visit(MemberAssignNode* node) {
    std::string structName = var_struct_type[node->objectName];
    int fieldIdx = struct_field_indices[structName + "." + node->fieldName];
    std::string fieldType = struct_field_types[structName + "." + node->fieldName];
    
    std::string ptr_reg = getTempReg();
    emit("  " + ptr_reg + " = getelementptr %" + structName + ", %" + structName + "* %" + node->objectName + ", i32 0, i32 " + std::to_string(fieldIdx));
    
    std::string val_reg = evaluateExpression(node->expr.get());
    emit("  store " + fieldType + " " + val_reg + ", " + fieldType + "* " + ptr_reg + ", align 4");
}

void LLVMCodeGen::visit(ProgramNode* node) {
    std::cout << "[ALU LLVM CodeGen] Translating AST to LLVM IR (Text Form)..." << std::endl;
    
    emit("; ModuleID = 'alu_module'");
    emit("source_filename = \"alu_source.alu\"");
    emit("target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
    emit("target triple = \"x86_64-pc-windows-msvc\"\n");
    emit("declare void @free(i8*)\n");
    
    for (const auto& decl : node->declarations) {
        decl->codegen(*this);
    }
    
    std::cout << "[ALU LLVM CodeGen] Translation Complete." << std::endl;
}
