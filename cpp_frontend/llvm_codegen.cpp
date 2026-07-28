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
    return "%" + std::to_string(tmp_counter++);
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
void RoutineNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ProgramNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }

// --- LLVMCodeGen Visitor Implementation --- //

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

std::string LLVMCodeGen::visit(LiteralNode* node) {
    if (node->type == DataType::INT) {
        return node->value;
    } else if (node->type == DataType::STRING) {
        // Mocking string pointer resolution in LLVM IR
        // In real LLVM IR, this would be a GEP to a global string constant.
        return "c\"" + node->value + "\\00\""; 
    }
    return "0";
}

std::string LLVMCodeGen::visit(BinOpNode* node) {
    // First generate code for left and right branches
    // This requires dynamic_casting in our simple AST structure to get return values
    std::string lval = "";
    std::string rval = "";

    if (auto litL = dynamic_cast<LiteralNode*>(node->left.get())) lval = visit(litL);
    if (auto litR = dynamic_cast<LiteralNode*>(node->right.get())) rval = visit(litR);

    // If it's an integer addition:
    std::string res_reg = getTempReg();
    if (node->op == "+") {
        // emit: %1 = add i32 5, 5
        emit("  " + res_reg + " = add i32 " + lval + ", " + rval);
    }
    return res_reg;
}

void LLVMCodeGen::visit(VarDeclNode* node) {
    // 1. Allocate memory on the stack (alloca)
    // emit: %x = alloca i32
    std::string llvm_type = (node->varType == "int") ? "i32" : "i8*";
    emit("  %" + node->name + " = alloca " + llvm_type + ", align 4");
    
    if (node->initializer) {
        std::string init_val = "";
        
        // Compute the initializer expression
        if (auto binop = dynamic_cast<BinOpNode*>(node->initializer.get())) {
            init_val = visit(binop);
        } else if (auto lit = dynamic_cast<LiteralNode*>(node->initializer.get())) {
            init_val = visit(lit);
        }

        // 2. Store the value into the allocated pointer
        // emit: store i32 %1, i32* %x
        if (init_val != "") {
            emit("  store " + llvm_type + " " + init_val + ", " + llvm_type + "* %" + node->name + ", align 4");
        }
    }
}

void LLVMCodeGen::visit(RoutineNode* node) {
    // define i32 @main() {
    // entry:
    std::string ret_type = (node->name == "main") ? "i32" : "void";
    emit("define " + ret_type + " @" + node->name + "() {");
    emit("entry:");
    
    for (const auto& stmt : node->body) {
        stmt->codegen(*this);
    }
    
    if (ret_type == "i32") {
        emit("  ret i32 0");
    } else {
        emit("  ret void");
    }
    emit("}\n");
}

void LLVMCodeGen::visit(ProgramNode* node) {
    std::cout << "[ALU LLVM CodeGen] Translating AST to LLVM IR (Text Form)..." << std::endl;
    
    emit("; ModuleID = 'alu_module'");
    emit("source_filename = \"alu_source.alu\"");
    emit("target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
    emit("target triple = \"x86_64-pc-windows-msvc\"\n");
    
    // Declare the C-standard puts function so we can link against msvcrt
    emit("declare i32 @puts(i8*)\n");
    
    for (const auto& decl : node->declarations) {
        decl->codegen(*this);
    }
    
    std::cout << "[ALU LLVM CodeGen] Translation Complete." << std::endl;
}
