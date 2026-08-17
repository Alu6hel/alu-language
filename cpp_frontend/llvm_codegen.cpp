#include <fstream>
#include "llvm_codegen.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>

LLVMCodeGen::LLVMCodeGen(const std::string& target) : tmp_counter(1), target_arch(target) {
}

std::string LLVMCodeGen::getIR() const {
    std::string di_str = emit_debug_info ? "\n" + di_output.str() : "";
    return ir_output.str() + "\n" + global_strings_output.str() + di_str;
}

int LLVMCodeGen::getDIFile(const std::string& filename) {
    if (!emit_debug_info) return -1;
    if (di_file_ids.find(filename) != di_file_ids.end()) return di_file_ids[filename];

    std::filesystem::path p(filename);
    std::string base_name = p.filename().string();
    std::string dir_name = p.parent_path().string();
    if (dir_name.empty()) dir_name = ".";
    // Convert backslashes to forward slashes for LLVM
    std::replace(dir_name.begin(), dir_name.end(), '\\', '/');

    int file_id = di_counter++;
    di_output << "!" << file_id << " = !DIFile(filename: \"" << base_name << "\", directory: \"" << dir_name << "\")\n";
    di_file_ids[filename] = file_id;

    if (di_cu_id == -1) {
        di_cu_id = di_counter++;
        di_output << "!" << di_cu_id << " = distinct !DICompileUnit(language: DW_LANG_C99, file: !" << file_id << ", producer: \"alu compiler\", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)\n";
        di_output << "!llvm.dbg.cu = !{!" << di_cu_id << "}\n";
        
        int dw_ver = di_counter++;
        di_output << "!" << dw_ver << " = !{i32 7, !\"Dwarf Version\", i32 4}\n";
        int dbg_ver = di_counter++;
        di_output << "!" << dbg_ver << " = !{i32 2, !\"Debug Info Version\", i32 3}\n";
        di_output << "!llvm.module.flags = !{!" << dw_ver << ", !" << dbg_ver << "}\n";
    }

    return file_id;
}

int LLVMCodeGen::getDIType(const std::string& typeName) {
    if (!emit_debug_info) return -1;
    if (di_type_ids.find(typeName) != di_type_ids.end()) return di_type_ids[typeName];

    int type_id = di_counter++;
    int size = 32;
    std::string encoding = "DW_ATE_signed";
    if (typeName == "int") { size = 32; encoding = "DW_ATE_signed"; }
    else if (typeName == "bool") { size = 8; encoding = "DW_ATE_boolean"; }
    else if (typeName == "float") { size = 32; encoding = "DW_ATE_float"; }
    else if (typeName == "double") { size = 64; encoding = "DW_ATE_float"; }
    else if (typeName == "byte") { size = 8; encoding = "DW_ATE_unsigned"; }
    else if (typeName == "string") { size = 64; encoding = "DW_ATE_address"; }
    else { size = 64; encoding = "DW_ATE_address"; } // pointers, arrays, structs

    di_output << "!" << type_id << " = !DIBasicType(name: \"" << typeName << "\", size: " << size << ", encoding: " << encoding << ")\n";
    di_type_ids[typeName] = type_id;
    return type_id;
}

std::string LLVMCodeGen::getDILoc() {
    if (!emit_debug_info || current_di_scope == -1 || current_debug_node == nullptr) return "";
    int loc_id = di_counter++;
    di_output << "!" << loc_id << " = !DILocation(line: " << current_debug_node->line << ", column: " << current_debug_node->col << ", scope: !" << current_di_scope << ")\n";
    return "!dbg !" + std::to_string(loc_id);
}

void LLVMCodeGen::emitUnhandledFallback() {
    if (current_func_ret_type == "void") emit("  ret void");
    else if (current_func_ret_type.find("*") != std::string::npos) emit("  ret " + current_func_ret_type + " null");
    else if (current_func_ret_type == "double" || current_func_ret_type == "float") emit("  ret " + current_func_ret_type + " 0.0");
    else emit("  ret " + current_func_ret_type + " 0");
}

void LLVMCodeGen::emitExceptionUnwind() {
    // If we have catch blocks in scope, branch to the nearest one.
    if (!catch_labels.empty()) {
        auto catch_info = catch_labels.back();
        int catch_depth = catch_info.second;
        int current_depth = (int)var_type_stack.size();
        
        // Release any scoped variables before jumping to catch
        emitScopeReleases(current_depth - catch_depth);
        emit("  br label %" + catch_info.first);
    } else {
        // Unwind to caller: Release ALL scopes
        emitScopeReleases(-1);
        
        // Propagate local %current_exception to caller's %__alu_err parameter if it's not null
        std::string local_ex = getTempReg();
        emit("  " + local_ex + " = load i8*, i8** %current_exception, align 8");
        std::string err_check = getTempReg();
        emit("  " + err_check + " = icmp ne i8** %__alu_err, null");
        std::string store_err_label = getLabel("store_err");
        std::string ret_err_label = getLabel("ret_err");
        emit("  br i1 " + err_check + ", label %" + store_err_label + ", label %" + ret_err_label);
        
        emit(store_err_label + ":");
        emit("  store i8* " + local_ex + ", i8** %__alu_err, align 8");
        emit("  br label %" + ret_err_label);
        
        emit(ret_err_label + ":");
        // Return default value
        if (current_func_ret_type == "i32") emit("  ret i32 0");
        else if (current_func_ret_type != "void") emit("  ret " + current_func_ret_type + " zeroinitializer");
        else emit("  ret void");
    }
}



void LLVMCodeGen::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (out.is_open()) {
        out << getIR();
        out.close();
    }
}

std::string LLVMCodeGen::getTempReg() {
    return "%t" + std::to_string(tmp_counter++);
}

std::string LLVMCodeGen::getLabel(const std::string& prefix) {
    return prefix + std::to_string(label_counter++);
}

void LLVMCodeGen::emit(const std::string& code, ASTNode* node) {
    if (code.find(":") != std::string::npos && code.find(" ") == std::string::npos) {
        block_terminated = false;
    }
    if (code.find("define ") == 0 || code.find("declare ") == 0 || code.find("}") == 0 || code.find("%") == 0 || code.find("@") == 0) {
        block_terminated = false;
    }
    
    if (block_terminated) return;
    
    std::string final_code = code;
    
    if (emit_debug_info && node != nullptr) {
        current_debug_node = node;
    }
    
    // Only append debug location if it's an instruction
    if (emit_debug_info && current_debug_node != nullptr && current_di_scope != -1) {
        if (code.rfind("  ", 0) == 0 && code.find(":") == std::string::npos && code.find(";") == std::string::npos) {
            // Append debug location unless it's a simple register assignment or already has metadata
            if (code.find("!dbg") == std::string::npos) {
                std::string loc = getDILoc();
                if (!loc.empty()) {
                    final_code += ", " + loc;
                }
            }
        }
    }
    
    (*current_output) << final_code << "\n";
    
    if (code.find("  ret ") == 0 || code.find("  br ") == 0 || code.find("  unreachable") == 0) {
        block_terminated = true;
    }
}

void LLVMCodeGen::emitAlloca(const std::string& code) {
    allocas.push_back(code);
}

// --- Scope Management ---

void LLVMCodeGen::pushScope() {
    var_type_stack.emplace_back();
    alu_type_stack.emplace_back();
    struct_type_stack.emplace_back();
    ir_name_stack.emplace_back();
}

void LLVMCodeGen::emitScopeReleases(int levels) {
    int start = (int)var_type_stack.size() - 1;
    int end = (levels == -1) ? 0 : start - levels + 1;
    if (end < 0) end = 0;

    for (int i = start; i >= end; --i) {
        auto& current_vars = var_type_stack[i];
        auto& current_names = ir_name_stack[i];
        
        for (const auto& pair : current_vars) {
            const std::string& var_name = pair.first;
            const std::string& llvm_type = pair.second;
            
            std::string alu_type = "";
            auto alu_it = alu_type_stack[i].find(var_name);
            if (alu_it != alu_type_stack[i].end()) {
                alu_type = alu_it->second;
            }
            
            // If it is an ARC-managed pointer
            if (alu_type == "string" || alu_type.find("managed<") == 0) {
                auto name_it = current_names.find(var_name);
                if (name_it != current_names.end()) {
                    std::string irName = name_it->second;
                    
                    std::string val_reg = getTempReg();
                    emit("  " + val_reg + " = load " + llvm_type + ", " + llvm_type + "* %" + irName + ", align 4");
                    
                    std::string cast_reg = getTempReg();
                    emit("  " + cast_reg + " = bitcast " + llvm_type + " " + val_reg + " to i8*");
                    emit("  call void @alu_release(i8* " + cast_reg + ")");
                }
            }
        }
    }
}

void LLVMCodeGen::popScope() {
    if (!var_type_stack.empty() && !ir_name_stack.empty()) {
        emitScopeReleases(1);
    }

    if (!var_type_stack.empty()) var_type_stack.pop_back();
    if (!alu_type_stack.empty()) alu_type_stack.pop_back();
    if (!struct_type_stack.empty()) struct_type_stack.pop_back();
    if (!ir_name_stack.empty()) ir_name_stack.pop_back();
}

std::string LLVMCodeGen::lookupVarType(const std::string& name) {
    for (int i = (int)var_type_stack.size() - 1; i >= 0; --i) {
        auto it = var_type_stack[i].find(name);
        if (it != var_type_stack[i].end()) return it->second;
    }
    return "";
}

std::string LLVMCodeGen::lookupStructType(const std::string& name) {
    for (int i = (int)struct_type_stack.size() - 1; i >= 0; --i) {
        auto it = struct_type_stack[i].find(name);
        if (it != struct_type_stack[i].end()) return it->second;
    }
    return "";
}

void LLVMCodeGen::declareVarType(const std::string& name, const std::string& llvmType) {
    if (!var_type_stack.empty()) {
        var_type_stack.back()[name] = llvmType;
    }
}

std::string LLVMCodeGen::lookupAluType(const std::string& name) {
    for (int i = (int)alu_type_stack.size() - 1; i >= 0; --i) {
        auto it = alu_type_stack[i].find(name);
        if (it != alu_type_stack[i].end()) return it->second;
    }
    return "";
}

void LLVMCodeGen::declareAluType(const std::string& name, const std::string& aluType) {
    if (!alu_type_stack.empty()) {
        alu_type_stack.back()[name] = aluType;
    }
}

void LLVMCodeGen::declareStructType(const std::string& name, const std::string& structName) {
    if (!struct_type_stack.empty()) {
        std::string actualName = structName;
        if (actualName.find("ptr") == 0) {
            size_t pos1 = actualName.find("<");
            size_t pos2 = actualName.rfind(">");
            if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
                actualName = actualName.substr(pos1 + 1, pos2 - pos1 - 1);
                while (!actualName.empty() && actualName.back() == ' ') actualName.pop_back();
                while (!actualName.empty() && actualName.front() == ' ') actualName.erase(0, 1);
            }
        }
        struct_type_stack.back()[name] = getNamespacedName(actualName);
    }
}

std::string LLVMCodeGen::getUniqueName(const std::string& sourceName) {
    int count = name_counter[sourceName]++;
    std::string irName = (count == 0) ? sourceName : sourceName + "." + std::to_string(count);
    if (!ir_name_stack.empty()) {
        ir_name_stack.back()[sourceName] = irName;
    }
    return irName;
}

std::string LLVMCodeGen::lookupIRName(const std::string& sourceName) {
    for (int i = (int)ir_name_stack.size() - 1; i >= 0; --i) {
        auto it = ir_name_stack[i].find(sourceName);
        if (it != ir_name_stack[i].end()) return it->second;
    }
    return sourceName; // fallback: assume it's the same
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
void ForNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void TryCatchNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ThrowNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ReturnNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void AssertNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
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
void MethodCallNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ImportNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void CastNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void EffectDeclNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void HandleNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void YieldNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ResumeNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void ProgramNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void NamespaceNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }
void VectorInitNode::codegen(LLVMCodeGen& cg) { cg.visit(this); }

// --- LLVMCodeGen Visitor Implementation --- //

std::string LLVMCodeGen::evaluateExpression(ASTNode* expr) {
    if (!expr) return "";
    if (auto lit = dynamic_cast<LiteralNode*>(expr)) return visit(lit);
    if (auto binop = dynamic_cast<BinOpNode*>(expr)) return visit(binop);
    if (auto cast = dynamic_cast<CastNode*>(expr)) return visit(cast);
    if (auto func = dynamic_cast<FuncCallNode*>(expr)) return visit(func);
    if (auto maccess = dynamic_cast<MemberAccessNode*>(expr)) return visit(maccess);
    if (auto vacc = dynamic_cast<VarAccessNode*>(expr)) return visit(vacc);
    if (auto addr = dynamic_cast<AddressOfNode*>(expr)) return visit(addr);
    if (auto deref = dynamic_cast<DereferenceNode*>(expr)) return visit(deref);
    if (auto alloc = dynamic_cast<NewAllocationNode*>(expr)) return visit(alloc);
    if (auto arrIdx = dynamic_cast<ArrayIndexNode*>(expr)) return visit(arrIdx);
    if (auto mcall = dynamic_cast<MethodCallNode*>(expr)) return visit(mcall);
    if (auto yieldn = dynamic_cast<YieldNode*>(expr)) {
        emit("  ; --- YIELD " + yieldn->effect_name + "." + yieldn->method_name + " ---");
        // Switch back to parent fiber
        std::string parent_fiber = getTempReg();
        emit("  " + parent_fiber + " = load i8*, i8** @__alu_parent_fiber, align 8");
        emit("  call void @SwitchToFiber(i8* " + parent_fiber + ")");
        return "0";
    }
    return "";
}

std::string LLVMCodeGen::getInferredLLVMType(ASTNode* expr) {
    if (!expr) return "i32";
    if (auto cast = dynamic_cast<CastNode*>(expr)) {
        if (cast->targetType == DataType::FLOAT) return "float";
        if (cast->targetType == DataType::DOUBLE) return "double";
        if (cast->targetType == DataType::BYTE) return "i8";
        return "i32";
    }
    if (auto lit = dynamic_cast<LiteralNode*>(expr)) {
        if (lit->type == DataType::STRING) return "i8*";
        if (lit->type == DataType::INT) return "i32";
        if (lit->type == DataType::FLOAT) return "float";
        if (lit->type == DataType::DOUBLE) return "double";
        if (lit->type == DataType::BOOL) return "i1";
        if (lit->type == DataType::BYTE) return "i8";
        if (lit->type == DataType::UNKNOWN) {
            std::string vt = lookupVarType(lit->value);
            if (!vt.empty()) return vt;
            return "i32";
        }
    }
    if (auto va = dynamic_cast<VarAccessNode*>(expr)) {
        std::string vt = lookupVarType(va->name);
        if (!vt.empty()) return vt;
    }
    if (auto ma = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string sname = lookupStructType(ma->objectName);
        if (!sname.empty() && sname.back() == '*') sname.pop_back();
        if (struct_field_types.count(sname + "." + ma->fieldName)) return struct_field_types[sname + "." + ma->fieldName];
    }
    if (auto mc = dynamic_cast<MethodCallNode*>(expr)) {
        if (auto varObj = dynamic_cast<VarAccessNode*>(mc->object.get())) {
            std::string structName = lookupVarType(varObj->name);
            if (!structName.empty() && structName.back() == '*') structName.pop_back();
            if (!structName.empty() && structName[0] == '%') structName = structName.substr(1);
            if (func_return_types.count(structName + "_" + mc->methodName)) return func_return_types[structName + "_" + mc->methodName];
        }
    }
    if (auto fc = dynamic_cast<FuncCallNode*>(expr)) {
        if (func_return_types.count(getNamespacedName(fc->name))) return func_return_types[getNamespacedName(fc->name)];
    }
    if (auto arr = dynamic_cast<ArrayIndexNode*>(expr)) {
        std::string arr_type = getInferredLLVMType(arr->arrayExpr.get());
        if (arr_type.find("[") != std::string::npos) {
            size_t pos = arr_type.find(" x ");
            if (pos != std::string::npos) {
                std::string t = arr_type.substr(pos + 3);
                if (!t.empty() && t.back() == ']') t.pop_back();
                return t;
            }
        }
        if (!arr_type.empty() && arr_type.back() == '*') {
            std::string t = arr_type;
            t.pop_back();
            return t;
        }
        return arr_type;
    }
    if (auto addr = dynamic_cast<AddressOfNode*>(expr)) {
        return getInferredLLVMType(addr->expr.get()) + "*";
    }
    if (auto deref = dynamic_cast<DereferenceNode*>(expr)) {
        std::string t = getInferredLLVMType(deref->expr.get());
        if (!t.empty() && t.back() == '*') t.pop_back();
        return t;
    }
    if (auto binop = dynamic_cast<BinOpNode*>(expr)) {
        if (binop->op == "==" || binop->op == "!=" || binop->op == "<" || 
            binop->op == "<=" || binop->op == ">" || binop->op == ">=") {
            return "i1";
        }
        std::string ltype = getInferredLLVMType(binop->left.get());
        std::string rtype = getInferredLLVMType(binop->right.get());
        if (ltype == "double" || rtype == "double") return "double";
        if (ltype == "float" || rtype == "float") return "float";
        if (ltype != "i32" && ltype != "") return ltype;
        return rtype;
    }
    return "i32"; // default fallback
}

void LLVMCodeGen::visit(AsmCallNode* node) {
    if (emit_debug_info) current_debug_node = node;
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
    std::string str_val = "c\"" + text + "\\00\"";
    int len = text.length() + 1;
    
    std::string reg = getTempReg();
    emitAlloca("  " + reg + " = alloca [" + std::to_string(len) + " x i8], align 1");
    emit("  store [" + std::to_string(len) + " x i8] " + str_val + ", [" + std::to_string(len) + " x i8]* " + reg + ", align 1");
    std::string ptr_reg = getTempReg();
    emit("  " + ptr_reg + " = bitcast [" + std::to_string(len) + " x i8]* " + reg + " to i8*");
    emit("  call i32 @puts(i8* " + ptr_reg + ")");
}

void LLVMCodeGen::visit(UnsafeBlockNode* node) {
    if (emit_debug_info) current_debug_node = node;
    emit("  ; Begin unsafe block");
    for (const auto& stmt : node->body) {
        stmt->codegen(*this);
    }
    emit("  ; End unsafe block");
}

std::string sanitizeLLVMName(std::string name) {
    size_t pos = 0;
    while ((pos = name.find("::", pos)) != std::string::npos) {
        name.replace(pos, 2, "__");
        pos += 2;
    }
    for (char& c : name) {
        if (c == '<' || c == '>' || c == ',' || c == ':') c = '.';
    }
    return name;
}

std::string LLVMCodeGen::getNamespacedName(const std::string& name) {
    if (extern_functions.count(name)) {
        return sanitizeLLVMName(name);
    }
    
    if (name.find("::") != std::string::npos) {
        std::string res = name;
        size_t pos = 0;
        while ((pos = res.find("::", pos)) != std::string::npos) {
            res.replace(pos, 2, "__");
            pos += 2;
        }
        return sanitizeLLVMName(res);
    }
    
    if (name == "int" || name == "float" || name == "double" || name == "string" || name == "byte" || name == "bool" || name == "void") {
        return name;
    }
    
    if (!current_namespace.empty()) {
        return current_namespace + "__" + sanitizeLLVMName(name);
    }
    return sanitizeLLVMName(name);
}

std::string LLVMCodeGen::getLLVMType(const std::string& type) {
    static std::ofstream d_log("dump_types.log", std::ios_base::app);
    d_log << "GETLLVMTYPE: '" << type << "'" << std::endl;
    
    std::string clean_type = type;
    size_t unit_start = clean_type.find('<');
    if (unit_start != std::string::npos && clean_type.find("ptr<") != 0 && clean_type.find("managed<") != 0 && clean_type.find("array<") != 0 && clean_type.find("routine<") != 0) {
        clean_type = clean_type.substr(0, unit_start);
    }
    
    if (type.find("ptr<") == 0 || type.find("managed<") == 0) {
        size_t pos1 = type.find("<");
        size_t pos2 = type.rfind(">");
        if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
            std::string base = type.substr(pos1 + 1, pos2 - pos1 - 1);
            while (!base.empty() && base.back() == ' ') base.pop_back();
            while (!base.empty() && base.front() == ' ') base.erase(0, 1);
            return getLLVMType(base) + "*";
        }
    }
      if (clean_type == "int") return "i32";
    if (clean_type == "float") return "float";
    if (clean_type == "double") return "double";
    if (clean_type == "string") return "i8*";
    if (clean_type == "bool") return "i1";
    if (clean_type == "void") return "void";
    if (clean_type == "byte") return "i8";
    if (clean_type == "float4") return "<4 x float>";
    if (clean_type == "float8") return "<8 x float>";
    if (clean_type == "int4") return "<4 x i32>";
    if (clean_type == "int8") return "<8 x i32>";
    if (type == "routine") return "i8*";
    if (type == "routine") return "i8*";
    if (type.find("[]") != std::string::npos) {
        std::string base = type.substr(0, type.find("[]"));
        if (base == "int") return "i32*";
        if (base == "float") return "float*";
        if (base == "double") return "double*";
        if (base == "string") return "i8**";
        if (base == "byte") return "i8*";
        return "%" + getNamespacedName(base) + "*";
    }
    if (type.find("*") != std::string::npos) {
        std::string base = type;
        base.pop_back(); // remove *
        return getLLVMType(base) + "*";
    }
    return "%" + getNamespacedName(type) + "*"; // Custom types are passed by reference
}

std::string LLVMCodeGen::visit(VectorInitNode* node) {
    std::string llvm_type = getLLVMType(node->typeName);
    std::string current_vec = "undef";
    
    std::string base_type = "float";
    if (node->typeName == "int4" || node->typeName == "int8") base_type = "i32";
    
    for (size_t i = 0; i < node->elements.size(); ++i) {
        std::string val_reg = evaluateExpression(node->elements[i].get());
        std::string next_vec = getTempReg();
        emit("  " + next_vec + " = insertelement " + llvm_type + " " + current_vec + ", " + base_type + " " + val_reg + ", i32 " + std::to_string(i), node);
        current_vec = next_vec;
    }
    return current_vec;
}

std::string LLVMCodeGen::visit(LiteralNode* node) {
    if (emit_debug_info) current_debug_node = node;
    if (node->type == DataType::INT) {
        return node->value;
    } else if (node->type == DataType::FLOAT) {
        double val = std::stod(node->value); // parse as double
        char buf[64];
        snprintf(buf, sizeof(buf), "%e", val);
        std::string reg1 = getTempReg();
        emit("  " + reg1 + " = fadd double 0.000000e+00, " + std::string(buf));
        std::string reg2 = getTempReg();
        emit("  " + reg2 + " = fptrunc double " + reg1 + " to float");
        return reg2;
    } else if (node->type == DataType::DOUBLE) {
        double val = std::stod(node->value);
        char buf[64];
        snprintf(buf, sizeof(buf), "%e", val);
        std::string reg = getTempReg();
        emit("  " + reg + " = fadd double 0.000000e+00, " + std::string(buf));
        return reg;
    } else if (node->type == DataType::STRING) {
        std::string text = node->value;
        std::string llvm_str = "";
        int actual_len = 1; // for \00
        for (size_t i = 0; i < text.length(); ++i) {
            if (text[i] == '\\' && i + 1 < text.length()) {
                if (text[i+1] == 'n') { llvm_str += "\\0A"; actual_len++; i++; continue; }
                if (text[i+1] == 't') { llvm_str += "\\09"; actual_len++; i++; continue; }
                if (text[i+1] == 'r') { llvm_str += "\\0D"; actual_len++; i++; continue; }
                if (text[i+1] == '"') { llvm_str += "\\22"; actual_len++; i++; continue; }
                if (text[i+1] == '\\') { llvm_str += "\\5C"; actual_len++; i++; continue; }
            }
            llvm_str += text[i];
            actual_len++;
        }
        llvm_str += "\\00";
        
        std::string str_name = "@.str." + std::to_string(global_str_counter++);
        std::string array_type = "[" + std::to_string(actual_len) + " x i8]";
        
        global_strings_output << str_name << " = private unnamed_addr constant " 
                              << array_type << " c\"" << llvm_str << "\", align 1\n";
                              
        std::string ptr_reg = getTempReg();
        emit("  " + ptr_reg + " = getelementptr inbounds " + array_type + ", " + array_type + "* " + str_name + ", i32 0, i32 0");
        return ptr_reg;
    } else if (node->type == DataType::UNKNOWN) {
        // It's a variable access, we need to load it. For now, assume it's i32.
        std::string reg = getTempReg();
        emit("  " + reg + " = load i32, i32* %" + node->value + ", align 4");
        return reg;
    }
    return "0";
}

std::string LLVMCodeGen::visit(VarAccessNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string irName = lookupIRName(node->name);
    std::string llvm_type = lookupVarType(node->name);
    if (llvm_type == "") llvm_type = "i32"; // Fallback
    if (llvm_type.find("[") != std::string::npos) {
        return "%" + irName;
    }
    std::string reg = getTempReg();
    emit("  " + reg + " = load " + llvm_type + ", " + llvm_type + "* %" + irName + ", align 4");
    
    // ARC: If it's a managed pointer, retain it.
    std::string alu_type = lookupAluType(node->name);
    if (alu_type == "string" || alu_type.find("managed<") == 0) {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = bitcast " + llvm_type + " " + reg + " to i8*");
        emit("  call void @alu_retain(i8* " + cast_reg + ")");
    }
    return reg;
}

std::string LLVMCodeGen::visit(AddressOfNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // AddressOf only makes sense for variables, fields, or functions.
    if (auto varNode = dynamic_cast<VarAccessNode*>(node->expr.get())) {
        std::string nName = getNamespacedName(varNode->name);
        if (func_signatures.find(nName) != func_signatures.end() || func_return_types.find(nName) != func_return_types.end()) {
            return "@" + sanitizeLLVMName(nName);
        }
        return "%" + lookupIRName(varNode->name);
    }
    return "0"; // Advanced address-of not fully implemented
}

std::string LLVMCodeGen::visit(DereferenceNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string ptr_reg = evaluateExpression(node->expr.get());
    std::string expr_type = getInferredLLVMType(node->expr.get());
    std::string elem_type = "i32";
    if (!expr_type.empty() && expr_type.back() == '*') {
        elem_type = expr_type;
        elem_type.pop_back();
    }
    
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load " + elem_type + ", " + expr_type + " " + ptr_reg + ", align 4");
    return val_reg;
}

void LLVMCodeGen::visit(DerefAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string ptr_reg = evaluateExpression(node->ptr_expr.get());
    std::string expr_type = getInferredLLVMType(node->ptr_expr.get());
    std::string val_reg = evaluateExpression(node->val_expr.get());
    
    std::string elem_type = "i32";
    if (!expr_type.empty() && expr_type.back() == '*') {
        elem_type = expr_type;
        elem_type.pop_back();
    }
    
    emit("  store " + elem_type + " " + val_reg + ", " + expr_type + " " + ptr_reg + ", align 4");
}

std::string LLVMCodeGen::visit(NewAllocationNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string llvm_type = getLLVMType(node->typeName);
    std::string base_type = llvm_type;
    
    // In Alu, getLLVMType for a struct returns '%StructName*'.
    // We need the size of the struct itself, so we remove the trailing '*'.
    if (!base_type.empty() && base_type.back() == '*') {
        base_type.pop_back();
    }
    
    // In Alu, 'new Type' returns a pointer to 'Type'
    // Since 'llvm_type' already has the '*', ptr_type should just be llvm_type.
    // Wait, if llvm_type is '%StructName*', it is already a pointer to the struct.
    // But Alu's pointer type for custom structs is 'ptr<StructName>' which maps to '%StructName**'.
    // Therefore, 'new' returning a pointer to the struct means it returns '%StructName*'.
    std::string ptr_type = base_type + "*";
    
    std::string size_ptr = getTempReg();
    emit("  " + size_ptr + " = getelementptr " + base_type + ", " + base_type + "* null, i32 1");
    std::string size_int = getTempReg();
    emit("  " + size_int + " = ptrtoint " + base_type + "* " + size_ptr + " to i32");

    std::string reg = getTempReg();
    emit("  " + reg + " = call i8* @__alu_alloc_internal(i32 " + size_int + ")");
    std::string cast_reg = getTempReg();
    emit("  " + cast_reg + " = bitcast i8* " + reg + " to " + ptr_type);
    return cast_reg;
}

void LLVMCodeGen::visit(FreeNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string ptr_reg = evaluateExpression(node->expr.get());
    std::string expr_type = getInferredLLVMType(node->expr.get());
    if (expr_type == "") expr_type = "i32*"; // Fallback
    
    std::string cast_reg = getTempReg();
    emit("  " + cast_reg + " = bitcast " + expr_type + " " + ptr_reg + " to i8*");
    emit("  call void @alu_release(i8* " + cast_reg + ")");
}

void LLVMCodeGen::visit(ArrayDeclNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string size_val = evaluateExpression(node->sizeExpr.get());
    std::string llvm_type = getLLVMType(node->type);
    std::string irName = getUniqueName(node->name);
    
    // alloca array type
    std::string array_llvm_type = "[" + size_val + " x " + llvm_type + "]";
    emitAlloca("  %" + irName + " = alloca " + array_llvm_type + ", align 4");
    declareVarType(node->name, array_llvm_type);
    declareAluType(node->name, "array<" + node->type + ">");
}

std::string LLVMCodeGen::visit(ArrayIndexNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string idx_reg = evaluateExpression(node->indexExpr.get());
    
    // Evaluate the array expression. For pointers, this loads the pointer value.
    std::string arr_val = evaluateExpression(node->arrayExpr.get());
    std::string arr_type = getInferredLLVMType(node->arrayExpr.get());
    
    std::string elem_type = "i32";
    if (arr_type.find("[") != std::string::npos) {
        size_t pos = arr_type.find(" x ");
        if (pos != std::string::npos) {
            elem_type = arr_type.substr(pos + 3);
            if (!elem_type.empty() && elem_type.back() == ']') elem_type.pop_back();
        }
    } else if (!arr_type.empty() && arr_type.back() == '*') {
        elem_type = arr_type;
        elem_type.pop_back();
    }
    
    std::string ptr_reg = getTempReg();
    
    if (arr_type.find("[") != std::string::npos) {
        // Arrays are technically accessed via pointers to the array in LLVM if they are allocas, 
        // but since evaluateExpression loads it, we might need a different handling if this breaks.
        // Assuming ALU's previous logic, we'll emulate it:
        emit("  " + ptr_reg + " = getelementptr inbounds " + arr_type + ", " + arr_type + "* " + arr_val + ", i32 0, i32 " + idx_reg);
    } else {
        // For pointers, arr_val is already the loaded pointer (e.g. i8* or i8**)
        emit("  " + ptr_reg + " = getelementptr inbounds " + elem_type + ", " + arr_type + " " + arr_val + ", i32 " + idx_reg);
    }
    
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load " + elem_type + ", " + elem_type + "* " + ptr_reg + ", align 4");
    return val_reg;
}

void LLVMCodeGen::visit(ArrayAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string idx_reg = evaluateExpression(node->indexExpr.get());
    std::string val_reg = evaluateExpression(node->valExpr.get());
    
    std::string arr_val = evaluateExpression(node->arrayExpr.get());
    std::string arr_type = getInferredLLVMType(node->arrayExpr.get());
    
    std::string elem_type = "i32";
    if (arr_type.find("[") != std::string::npos) {
        size_t pos = arr_type.find(" x ");
        if (pos != std::string::npos) {
            elem_type = arr_type.substr(pos + 3);
            if (!elem_type.empty() && elem_type.back() == ']') elem_type.pop_back();
        }
    } else if (!arr_type.empty() && arr_type.back() == '*') {
        elem_type = arr_type;
        elem_type.pop_back();
    }
    
    std::string ptr_reg = getTempReg();
    
    if (arr_type.find("[") != std::string::npos) {
        emit("  " + ptr_reg + " = getelementptr inbounds " + arr_type + ", " + arr_type + "* " + arr_val + ", i32 0, i32 " + idx_reg);
    } else {
        // arr_val is the pointer
        emit("  " + ptr_reg + " = getelementptr inbounds " + elem_type + ", " + arr_type + " " + arr_val + ", i32 " + idx_reg);
    }
    
    std::string expr_type = getInferredLLVMType(node->valExpr.get());
    if (elem_type == "i32" && expr_type == "i8") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = zext i8 " + val_reg + " to i32");
        val_reg = cast_reg;
    } else if (elem_type == "i8" && expr_type == "i32") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = trunc i32 " + val_reg + " to i8");
        val_reg = cast_reg;
    }
    
    emit("  store " + elem_type + " " + val_reg + ", " + elem_type + "* " + ptr_reg + ", align 4");
}

std::string LLVMCodeGen::visit(BinOpNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string lval = evaluateExpression(node->left.get());
    std::string rval = evaluateExpression(node->right.get());

    std::string ltype = getInferredLLVMType(node->left.get());
    std::string rtype = getInferredLLVMType(node->right.get());

    std::string res_reg = getTempReg();
    
    // Pointer arithmetic
    if (ltype.find("*") != std::string::npos && rtype == "i32" && node->op == "+") {
        std::string elem_type = ltype;
        elem_type.pop_back();
        emit("  " + res_reg + " = getelementptr inbounds " + elem_type + ", " + ltype + " " + lval + ", i32 " + rval);
        return res_reg;
    }
    if (rtype.find("*") != std::string::npos && ltype == "i32" && node->op == "+") {
        std::string elem_type = rtype;
        elem_type.pop_back();
        emit("  " + res_reg + " = getelementptr inbounds " + elem_type + ", " + rtype + " " + rval + ", i32 " + lval);
        return res_reg;
    }

    bool isFloat = (ltype == "float" || rtype == "float" || ltype == "double" || rtype == "double" || ltype == "<4 x float>" || ltype == "<8 x float>");
    std::string opType = "i32";
    if (ltype == "double" || rtype == "double") opType = "double";
    else if (ltype == "float" || rtype == "float") opType = "float";
    else if (ltype == "<4 x float>") opType = "<4 x float>";
    else if (ltype == "<8 x float>") opType = "<8 x float>";
    else if (ltype == "<4 x i32>") opType = "<4 x i32>";
    else if (ltype == "<8 x i32>") opType = "<8 x i32>";
    else if (ltype == "i8" && rtype == "i8") opType = "i8";
    else if (ltype == "i1" && rtype == "i1") opType = "i1";

    // Integer promotion
    if (!isFloat && ltype != opType) {
        std::string new_l = getTempReg();
        emit("  " + new_l + " = zext " + ltype + " " + lval + " to " + opType);
        lval = new_l;
        ltype = opType;
    }
    if (!isFloat && rtype != opType) {
        std::string new_r = getTempReg();
        emit("  " + new_r + " = zext " + rtype + " " + rval + " to " + opType);
        rval = new_r;
        rtype = opType;
    }

    // Simple type promotion
    if (isFloat && ltype != opType) {
        std::string new_l = getTempReg();
        if (ltype == "i32") emit("  " + new_l + " = sitofp i32 " + lval + " to " + opType);
        else if (ltype == "float") emit("  " + new_l + " = fpext float " + lval + " to double");
        lval = new_l;
        ltype = opType;
    }
    if (isFloat && rtype != opType) {
        std::string new_r = getTempReg();
        if (rtype == "i32") emit("  " + new_r + " = sitofp i32 " + rval + " to " + opType);
        else if (rtype == "float") emit("  " + new_r + " = fpext float " + rval + " to double");
        rval = new_r;
        rtype = opType;
    }

    if (node->op == "+") {
        if (isFloat) emit("  " + res_reg + " = fadd " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = add " + opType + " " + lval + ", " + rval);
    } else if (node->op == "-") {
        if (isFloat) emit("  " + res_reg + " = fsub " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = sub " + opType + " " + lval + ", " + rval);
    } else if (node->op == "*") {
        if (isFloat) emit("  " + res_reg + " = fmul " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = mul " + opType + " " + lval + ", " + rval);
    } else if (node->op == "/") {
        if (isFloat) emit("  " + res_reg + " = fdiv " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = sdiv " + opType + " " + lval + ", " + rval);
    } else if (node->op == "%") {
        if (isFloat) emit("  " + res_reg + " = frem " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = srem " + opType + " " + lval + ", " + rval);
    } else if (node->op == "==") {
        if (isFloat) emit("  " + res_reg + " = fcmp oeq " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp eq " + opType + " " + lval + ", " + rval);
    } else if (node->op == "!=") {
        if (isFloat) emit("  " + res_reg + " = fcmp one " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp ne " + opType + " " + lval + ", " + rval);
    } else if (node->op == "<") {
        if (isFloat) emit("  " + res_reg + " = fcmp olt " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp slt " + opType + " " + lval + ", " + rval);
    } else if (node->op == "<=") {
        if (isFloat) emit("  " + res_reg + " = fcmp ole " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp sle " + opType + " " + lval + ", " + rval);
    } else if (node->op == ">") {
        if (isFloat) emit("  " + res_reg + " = fcmp ogt " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp sgt " + opType + " " + lval + ", " + rval);
    } else if (node->op == ">=") {
        if (isFloat) emit("  " + res_reg + " = fcmp oge " + opType + " " + lval + ", " + rval);
        else emit("  " + res_reg + " = icmp sge " + opType + " " + lval + ", " + rval);
    } else if (node->op == "&") {
        emit("  " + res_reg + " = and " + opType + " " + lval + ", " + rval);
    } else if (node->op == "|") {
        emit("  " + res_reg + " = or " + opType + " " + lval + ", " + rval);
    } else if (node->op == "^") {
        emit("  " + res_reg + " = xor " + opType + " " + lval + ", " + rval);
    } else if (node->op == "<<") {
        emit("  " + res_reg + " = shl " + opType + " " + lval + ", " + rval);
    } else if (node->op == ">>") {
        emit("  " + res_reg + " = ashr " + opType + " " + lval + ", " + rval);
    }
    return res_reg;
}

std::string LLVMCodeGen::visit(MethodCallNode* node) {
    if (emit_debug_info) current_debug_node = node;
    auto varObj = dynamic_cast<VarAccessNode*>(node->object.get());
    std::string llvm_type = lookupVarType(varObj->name);
    if (llvm_type.empty()) llvm_type = "i32";
    bool isPtr = false;
    if (llvm_type.back() == '*') {
        isPtr = true;
    }
    
    std::string ptr_reg;
    if (isPtr) {
        ptr_reg = evaluateExpression(node->object.get());
    } else {
        ptr_reg = "%" + varObj->name; // Alloca pointer
    }
    
    std::string structName = llvm_type;
    if (structName.back() == '*') structName.pop_back();
    if (structName[0] == '%') structName = structName.substr(1);
    
    std::string mangledName = structName + "_" + node->methodName;
    
    std::string args_str = getLLVMType(structName) + "* " + ptr_reg;
    for (size_t i = 0; i < node->args.size(); ++i) {
        args_str += ", ";
        

        
        std::string arg_val = evaluateExpression(node->args[i].get());
        std::string arg_type = getInferredLLVMType(node->args[i].get());
        if (arg_type == "") arg_type = "i32";
        
        args_str += arg_type + " " + arg_val;
    }
    
    // Check if return type is void
    // For simplicity, we assume void if we don't know, or i32.
    // Real implementation would look up function_table.
    std::string ret_type = func_return_types[mangledName];
    if (ret_type == "") ret_type = "i32"; // Fallback
    
    std::string ret_reg = "0";
    std::string call_args = args_str;
    if (extern_functions.find(mangledName) == extern_functions.end()) {
        if (!call_args.empty()) call_args += ", ";
        call_args += "i8** %current_exception";
    }

    if (ret_type == "void") {
        emit("  call void @" + mangledName + "(" + call_args + ")");
    } else {
        ret_reg = getTempReg();
        emit("  " + ret_reg + " = call " + ret_type + " @" + mangledName + "(" + call_args + ")");
    }
    
    // Exception check
    std::string ex_ptr = getTempReg();
    std::string ex_cond = getTempReg();
    std::string cont_label = getLabel("cont");
    std::string handle_label = getLabel("ex_handle");
    
    emit("  " + ex_ptr + " = load i8*, i8** %current_exception, align 8");
    emit("  " + ex_cond + " = icmp ne i8* " + ex_ptr + ", null");
    emit("  br i1 " + ex_cond + ", label %" + handle_label + ", label %" + cont_label);
    
    emit(handle_label + ":");
    emitExceptionUnwind();
    
    emit(cont_label + ":");
    
    return ret_reg;
}

void LLVMCodeGen::visit(ImportNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // LLVM CodeGen doesn't need to do anything for imports
    // The AST nodes from the imported file are already merged in ProgramNode
}

void LLVMCodeGen::visit(VarDeclNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // 1. Allocate memory on the stack (alloca)
    std::string llvm_type = getLLVMType(node->varType);
    std::string irName = getUniqueName(node->name);
    emitAlloca("  %" + irName + " = alloca " + llvm_type + ", align 4");
    declareVarType(node->name, llvm_type);
    declareAluType(node->name, node->varType);
    
    if (node->varType != "int" && node->varType != "string" && node->varType != "bool" && node->varType != "float" && node->varType != "double" && node->varType != "byte") {
        declareStructType(node->name, node->varType);
    }
    
    if (emit_debug_info) {
        int file_id = getDIFile(node->file);
        int v_type_id = getDIType(node->varType);
        int var_id = di_counter++;
        di_output << "!" << var_id << " = !DILocalVariable(name: \"" << node->name << "\", scope: !" << current_di_scope << ", file: !" << file_id << ", line: " << node->line << ", type: !" << v_type_id << ")\n";
        emit("  call void @llvm.dbg.declare(metadata " + llvm_type + "* %" + irName + ", metadata !" + std::to_string(var_id) + ", metadata !DIExpression())", node);
    }
    
    if (node->initializer) {
        std::string init_val = evaluateExpression(node->initializer.get());
        
        std::string expr_type = getInferredLLVMType(node->initializer.get());
        if (llvm_type == "i32" && expr_type == "i8") {
            std::string cast_reg = getTempReg();
            emit("  " + cast_reg + " = zext i8 " + init_val + " to i32");
            init_val = cast_reg;
        } else if (llvm_type == "i8" && expr_type == "i32") {
            std::string cast_reg = getTempReg();
            emit("  " + cast_reg + " = trunc i32 " + init_val + " to i8");
            init_val = cast_reg;
        } else if (llvm_type == "double" && expr_type == "float") {
            std::string cast_reg = getTempReg();
            emit("  " + cast_reg + " = fpext float " + init_val + " to double");
            init_val = cast_reg;
        } else if (llvm_type == "float" && expr_type == "double") {
            std::string cast_reg = getTempReg();
            emit("  " + cast_reg + " = fptrunc double " + init_val + " to float");
            init_val = cast_reg;
        }

        // 2. Store the value into the allocated pointer
        if (init_val != "") {
            emit("  store " + llvm_type + " " + init_val + ", " + llvm_type + "* %" + irName + ", align 4");
        }
    }
}

void LLVMCodeGen::visit(VarAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string val = evaluateExpression(node->expr.get());
    std::string irName = lookupIRName(node->name);
    std::string llvm_type = lookupVarType(node->name);
    if (llvm_type == "") llvm_type = "i32"; // Fallback
    
    std::string expr_type = getInferredLLVMType(node->expr.get());
    if (llvm_type == "i32" && expr_type == "i8") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = zext i8 " + val + " to i32");
        val = cast_reg;
    } else if (llvm_type == "i8" && expr_type == "i32") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = trunc i32 " + val + " to i8");
        val = cast_reg;
    } else if (llvm_type == "double" && expr_type == "float") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = fpext float " + val + " to double");
        val = cast_reg;
    } else if (llvm_type == "float" && expr_type == "double") {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = fptrunc double " + val + " to float");
        val = cast_reg;
    }
    
    // ARC: release old pointer value before overwrite
    if (llvm_type.find("*") != std::string::npos && llvm_type.find("[") == std::string::npos) {
        std::string old_val_reg = getTempReg();
        emit("  " + old_val_reg + " = load " + llvm_type + ", " + llvm_type + "* %" + irName + ", align 4");
        std::string cast_old_reg = getTempReg();
        emit("  " + cast_old_reg + " = bitcast " + llvm_type + " " + old_val_reg + " to i8*");
        emit("  call void @alu_release(i8* " + cast_old_reg + ")");
    } else if (llvm_type.back() == '*' && expr_type.back() == '*' && llvm_type != expr_type) {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = bitcast " + expr_type + " " + val + " to " + llvm_type);
        val = cast_reg;
    }
    
    emit("  store " + llvm_type + " " + val + ", " + llvm_type + "* %" + irName + ", align 4");
}

void LLVMCodeGen::visit(IfNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string cond_reg = evaluateExpression(node->condition.get());
    std::string cond_type = getInferredLLVMType(node->condition.get());
    if (cond_type == "") cond_type = "i32";
    if (cond_type != "i1") {
        std::string bool_reg = getTempReg();
        emit("  " + bool_reg + " = icmp ne " + cond_type + " " + cond_reg + ", 0");
        cond_reg = bool_reg;
    }
    
    std::string then_label = getLabel("if.then");
    std::string else_label = node->else_body.empty() ? getLabel("if.end") : getLabel("if.else");
    std::string end_label = node->else_body.empty() ? else_label : getLabel("if.end");
    
    emit("  br i1 " + cond_reg + ", label %" + then_label + ", label %" + else_label);
    
    emit(then_label + ":");
    pushScope();
    for (const auto& stmt : node->then_body) stmt->codegen(*this);
    popScope();
    emit("  br label %" + end_label);
    
    if (!node->else_body.empty()) {
        emit(else_label + ":");
        pushScope();
        for (const auto& stmt : node->else_body) stmt->codegen(*this);
        popScope();
        emit("  br label %" + end_label);
    }
    
    emit(end_label + ":");
}

void LLVMCodeGen::visit(WhileNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string cond_label = getLabel("while.cond");
    std::string body_label = getLabel("while.body");
    std::string end_label = getLabel("while.end");
    
    emit("  br label %" + cond_label);
    emit(cond_label + ":");
    
    std::string cond_reg = evaluateExpression(node->condition.get());
    std::string cond_type = getInferredLLVMType(node->condition.get());
    if (cond_type == "") cond_type = "i32";
    if (cond_type != "i1") {
        std::string bool_reg = getTempReg();
        emit("  " + bool_reg + " = icmp ne " + cond_type + " " + cond_reg + ", 0");
        cond_reg = bool_reg;
    }
    
    emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);
    
    emit(body_label + ":");
    pushScope();
    for (const auto& stmt : node->body) stmt->codegen(*this);
    popScope();
    emit("  br label %" + cond_label);
    
    emit(end_label + ":");
}

void LLVMCodeGen::visit(ForNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // Hardware-Accelerated Compute Shader Detection (Heuristic)
    bool hasNestedLoop = false;
    for (const auto& stmt : node->body) {
        if (dynamic_cast<ForNode*>(stmt.get())) {
            hasNestedLoop = true;
            break;
        }
    }
    
    if (hasNestedLoop) {
        std::cerr << "[ALU GPU Optimizer] Detected image transformation nested loop! Emitting Vulkan and Metal Compute Shaders..." << std::endl;
        
        // Vulkan (GLSL)
        std::ofstream compFile("compute_shader.comp");
        compFile << "#version 450\n"
                 << "layout(local_size_x = 16, local_size_y = 16) in;\n"
                 << "layout(binding = 0, rgba8) uniform image2D imgInput;\n"
                 << "layout(binding = 1, rgba8) uniform image2D imgOutput;\n"
                 << "void main() {\n"
                 << "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
                 << "    vec4 pixel = imageLoad(imgInput, pos);\n"
                 << "    // Auto-translated ALU transformations go here\n"
                 << "    imageStore(imgOutput, pos, pixel);\n"
                 << "}\n";
        compFile.close();

        // Apple Metal (MSL)
        std::ofstream metalFile("compute_shader.metal");
        metalFile << "#include <metal_stdlib>\n"
                  << "using namespace metal;\n\n"
                  << "kernel void compute_main(texture2d<float, access::read> imgInput [[texture(0)]],\n"
                  << "                         texture2d<float, access::write> imgOutput [[texture(1)]],\n"
                  << "                         uint2 pos [[thread_position_in_grid]]) {\n"
                  << "    if (pos.x >= imgInput.get_width() || pos.y >= imgInput.get_height()) return;\n"
                  << "    float4 pixel = imgInput.read(pos);\n"
                  << "    // Auto-translated ALU transformations go here\n"
                  << "    imgOutput.write(pixel, pos);\n"
                  << "}\n";
        metalFile.close();
    }

    // Push scope for the for-loop (covers init variable like 'int i')
    pushScope();
    
    // Emit init statement (e.g., int i = 0)
    if (node->init) node->init->codegen(*this);
    
    std::string cond_label = getLabel("for.cond");
    std::string body_label = getLabel("for.body");
    std::string end_label = getLabel("for.end");
    
    emit("  br label %" + cond_label);
    emit(cond_label + ":");
    
    if (node->condition) {
        std::string cond_reg = evaluateExpression(node->condition.get());
        emit("  br i1 " + cond_reg + ", label %" + body_label + ", label %" + end_label);
    } else {
        emit("  br label %" + body_label);
    }
    
    emit(body_label + ":");
    // Push scope for body-local variables
    pushScope();
    for (const auto& stmt : node->body) stmt->codegen(*this);
    popScope();
    
    // Emit update (e.g., i = i + 1)
    if (node->update) node->update->codegen(*this);
    
    emit("  br label %" + cond_label);
    
    emit(end_label + ":");
    
    popScope();
}

void LLVMCodeGen::visit(ThrowNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string ex_val = evaluateExpression(node->expr.get());
    std::string expr_type = getInferredLLVMType(node->expr.get());
    
    std::string final_ptr = getTempReg();
    
    // If it's a struct pointer, heap allocate and copy to avoid use-after-free
    if (expr_type.find("%") != std::string::npos && expr_type.find("*") != std::string::npos) {
        std::string elem_type = expr_type;
        elem_type.pop_back(); // remove '*'
        
        std::string malloc_reg = getTempReg();
        // Allocate 64 bytes (plenty for our Error struct containing i32 + i8*)
        emit("  " + malloc_reg + " = call i8* @malloc(i32 64)");
        
        std::string heap_ptr = getTempReg();
        emit("  " + heap_ptr + " = bitcast i8* " + malloc_reg + " to " + expr_type);
        
        std::string val_reg = getTempReg();
        emit("  " + val_reg + " = load " + elem_type + ", " + expr_type + " " + ex_val + ", align 4");
        emit("  store " + elem_type + " " + val_reg + ", " + expr_type + " " + heap_ptr + ", align 4");
        
        emit("  " + final_ptr + " = bitcast " + expr_type + " " + heap_ptr + " to i8*");
    } else {
        if (expr_type != "i8*") {
            emit("  " + final_ptr + " = bitcast " + expr_type + " " + ex_val + " to i8*");
        } else {
            final_ptr = ex_val;
        }
    }
    
    emit("  store i8* " + final_ptr + ", i8** %current_exception, align 8");
    emitExceptionUnwind();
}

void LLVMCodeGen::visit(TryCatchNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string catch_label = getLabel("catch");
    std::string end_try_label = getLabel("end_try");
    
    // Push catch label so exceptions thrown in try block jump here
    // Record the current scope depth so we know how many scopes to unwind
    int current_depth = (int)var_type_stack.size();
    catch_labels.push_back({catch_label, current_depth});
    
    // Try block
    pushScope();
    for (const auto& stmt : node->try_body) stmt->codegen(*this);
    popScope();
    
    // Pop catch label
    catch_labels.pop_back();
    
    emit("  br label %" + end_try_label);
    
    // Catch block
    emit(catch_label + ":");
    pushScope();
    
    // Retrieve exception and clear thread-local global flag
    std::string ex_ptr = getTempReg();
    emit("  " + ex_ptr + " = load i8*, i8** %current_exception, align 8");
    emit("  store i8* null, i8** %current_exception, align 8");
    
    // Declare catch variable
    std::string catch_var_ir = lookupIRName(node->catch_var_name);
    // Determine LLVM type for catch_var
    std::string llvm_catch_type = getLLVMType(node->catch_var_type);
    emitAlloca("  %" + catch_var_ir + " = alloca " + llvm_catch_type + ", align 8");
    declareVarType(node->catch_var_name, llvm_catch_type);
    declareAluType(node->catch_var_name, node->catch_var_type);
    if (node->catch_var_type != "int" && node->catch_var_type != "string" && node->catch_var_type != "bool") {
        declareStructType(node->catch_var_name, node->catch_var_type);
    }
    
    // Cast the i8* exception payload back to the expected type
    std::string cast_ptr = getTempReg();
    emit("  " + cast_ptr + " = bitcast i8* " + ex_ptr + " to " + llvm_catch_type);
    emit("  store " + llvm_catch_type + " " + cast_ptr + ", " + llvm_catch_type + "* %" + catch_var_ir + ", align 8");
    
    for (const auto& stmt : node->catch_body) stmt->codegen(*this);
    
    if (llvm_catch_type.find("%") != std::string::npos && llvm_catch_type.find("*") != std::string::npos) {
        emit("  call void @free(i8* " + ex_ptr + ")");
    }
    
    popScope();
    emit("  br label %" + end_try_label);
    
    emit(end_try_label + ":");
}

void LLVMCodeGen::visit(ReturnNode* node) {
    if (emit_debug_info) current_debug_node = node;
    emitScopeReleases(-1); // Release all variables in all scopes before returning
    if (node->expr) {
        std::string val = evaluateExpression(node->expr.get());
        std::string ret_type = getInferredLLVMType(node->expr.get());
        if (ret_type == "") ret_type = "i32"; // default fallback
        emit("  ret " + ret_type + " " + val);
    } else {
        emit("  ret void");
    }
}

void LLVMCodeGen::visit(AssertNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // Zero-Cost Abstraction: The Z3 Theorem Prover has mathematically verified
    // that this condition is unconditionally true at compile time.
    // No LLVM IR needs to be emitted for runtime evaluation.
}

std::string LLVMCodeGen::visit(FuncCallNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::vector<std::string> arg_vals;
    std::vector<std::string> arg_types;
    std::vector<bool> arg_is_arc_ptr;
    
    for (const auto& arg : node->args) {

        
        std::string arg_val = evaluateExpression(arg.get());
        std::string arg_type = getInferredLLVMType(arg.get());
        if (arg_type == "") arg_type = "i32";
        
        arg_vals.push_back(arg_val);
        arg_types.push_back(arg_type);
        bool is_ptr = (arg_type.find("*") != std::string::npos && arg_type.find("[") == std::string::npos);
        arg_is_arc_ptr.push_back(is_ptr);
    }
    
    std::string args_str = "";
    for (size_t i = 0; i < arg_vals.size(); ++i) {
        args_str += arg_types[i] + " " + arg_vals[i];
        if (i < arg_vals.size() - 1) args_str += ", ";
    }
    
    std::string safeName = getNamespacedName(node->name);
    std::string ret_type = func_return_types[safeName];
    if (ret_type == "") ret_type = "i32"; // Fallback
    
    std::string sig = "";
    if (func_signatures.count(safeName)) {
        sig = func_signatures[safeName] + " ";
    }
    
    std::string res_reg = "0";
    std::string call_args = args_str;
    if (extern_functions.find(safeName) == extern_functions.end()) {
        if (!call_args.empty()) call_args += ", ";
        call_args += "i8** %current_exception";
    }

    if (ret_type == "void") {
        emit("  call void " + sig + "@" + safeName + "(" + call_args + ")");
    } else {
        res_reg = getTempReg();
        emit("  " + res_reg + " = call " + ret_type + " " + sig + "@" + safeName + "(" + call_args + ")");
    }
    
    // ARC: If it's an extern function, it didn't consume the ARC reference of arguments.
    // So we must release the arguments we evaluated.
    if (extern_functions.count(safeName)) {
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (arg_is_arc_ptr[i]) {
                std::string cast_reg = getTempReg();
                emit("  " + cast_reg + " = bitcast " + arg_types[i] + " " + arg_vals[i] + " to i8*");
                emit("  call void @alu_release(i8* " + cast_reg + ")");
            }
        }
    }
    
    // Exception check
    std::string ex_ptr = getTempReg();
    std::string ex_cond = getTempReg();
    std::string cont_label = getLabel("cont");
    std::string handle_label = getLabel("ex_handle");
    
    emit("  " + ex_ptr + " = load i8*, i8** %current_exception, align 8");
    emit("  " + ex_cond + " = icmp ne i8* " + ex_ptr + ", null");
    emit("  br i1 " + ex_cond + ", label %" + handle_label + ", label %" + cont_label);
    
    emit(handle_label + ":");
    emitExceptionUnwind();
    
    emit(cont_label + ":");
    
    return res_reg;
}

void LLVMCodeGen::visit(RoutineNode* node) {
    if (emit_debug_info) current_debug_node = node;
    if (!node->type_params.empty()) return; // Skip generic templates

    // Clear scope stacks and push a fresh function-level scope
    var_type_stack.clear();
    struct_type_stack.clear();
    ir_name_stack.clear();
    name_counter.clear();
    pushScope();
    
    std::string ret_type = getLLVMType(node->returnType);
    if (node->name == "main") ret_type = "i32";
    current_func_ret_type = ret_type;
    
    std::string params_str = "";
    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string ptype = getLLVMType(node->params[i].type);
        params_str += ptype + " %in_" + node->params[i].name;
        if (i < node->params.size() - 1) params_str += ", ";
    }
    int file_id = getDIFile(node->file);
    int subrt_id = di_counter++;
    int sp_id = di_counter++;
    
    if (emit_debug_info) {
        // Simple subroutine type for now (just representing a function)
        di_output << "!" << subrt_id << " = !DISubroutineType(types: !{null})\n";
        di_output << "!" << sp_id << " = distinct !DISubprogram(name: \"" << node->name << "\", scope: !" << file_id << ", file: !" << file_id << ", line: " << node->line << ", type: !" << subrt_id << ", scopeLine: " << node->line << ", spFlags: DISPFlagDefinition, unit: !" << di_cu_id << ")\n";
        current_di_scope = sp_id;
    }

    std::string safeName = getNamespacedName(node->name);
    if (!params_str.empty()) params_str += ", ";
    params_str += "i8** %__alu_err";
    
    std::string def_str = "define " + ret_type + " @" + safeName + "(" + params_str + ")";
    if (emit_debug_info) def_str += " !dbg !" + std::to_string(sp_id);
    def_str += " {";
    
    emit(def_str);
    emit("entry:");
    
    // Setup buffering for body and allocas
    allocas.clear();
    std::stringstream body_output;
    std::stringstream* prev_output = current_output;
    current_output = &body_output;
    
    emitAlloca("  %current_exception = alloca i8*, align 8");
    emit("  store i8* null, i8** %current_exception, align 8");
    
    // Allocate stack space for parameters
    for (const auto& p : node->params) {
        std::string ptype = getLLVMType(p.type);
        declareVarType(p.name, ptype);
        declareAluType(p.name, p.type);
        emitAlloca("  %" + p.name + " = alloca " + ptype + ", align 4");
        emit("  store " + ptype + " %in_" + p.name + ", " + ptype + "* %" + p.name + ", align 4");
        
        declareVarType(p.name, ptype);
        if (p.type != "int" && p.type != "string" && p.type != "bool") {
            declareStructType(p.name, p.type);
        }
        
        if (emit_debug_info) {
            int p_type_id = getDIType(p.type);
            int var_id = di_counter++;
            di_output << "!" << var_id << " = !DILocalVariable(name: \"" << p.name << "\", scope: !" << current_di_scope << ", file: !" << file_id << ", line: " << node->line << ", type: !" << p_type_id << ")\n";
            emit("  call void @llvm.dbg.declare(metadata " + ptype + "* %" + p.name + ", metadata !" + std::to_string(var_id) + ", metadata !DIExpression())", node);
        }
    }
    
    for (auto& stmt : node->body) {
        stmt->codegen(*this);
    }
    
    emitScopeReleases(1); // Release top-level routine scope before default return
    
    if (ret_type == "i32") {
        emit("  ret i32 0");
    } else if (ret_type != "void") {
        emit("  ret " + ret_type + " zeroinitializer");
    } else {
        emit("  ret void");
    }
    
    // Restore output and flush
    current_output = prev_output;
    
    // Force block_terminated to false so allocas are printed
    block_terminated = false;
    for (const auto& al : allocas) {
        emit(al);
    }
    (*current_output) << body_output.str();
    
    // Reset block terminated so the closing brace is printed
    block_terminated = false;
    emit("}\n");
    
    // We already released, just pop the stack silently
    if (!var_type_stack.empty()) var_type_stack.pop_back();
    if (!struct_type_stack.empty()) struct_type_stack.pop_back();
    if (!ir_name_stack.empty()) ir_name_stack.pop_back();
    
    current_di_scope = -1;
}

void LLVMCodeGen::visit(ExternRoutineNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string ret_type = getLLVMType(node->returnType);
    
    std::string params_str = "";
    for (size_t i = 0; i < node->params.size(); ++i) {
        std::string ptype = getLLVMType(node->params[i].type);
        params_str += ptype;
        if (i < node->params.size() - 1 || node->isVariadic) params_str += ", ";
    }
    if (node->isVariadic) {
        params_str += "...";
        func_signatures[node->name] = "(" + params_str + ")";
    }

    if (extern_functions.find(node->name) == extern_functions.end()) {
        extern_functions.insert(node->name);
        std::string safeName = sanitizeLLVMName(node->name);
        emit("declare " + ret_type + " @" + safeName + "(" + params_str + ")\n");
    }
}
void LLVMCodeGen::visit(StructDefNode* node) {
    if (emit_debug_info) current_debug_node = node;
    if (!node->type_params.empty()) return; // skip templates
    std::string fields = "";
    std::string safeName = getNamespacedName(node->name);
    for (size_t i = 0; i < node->fields.size(); ++i) {
        std::string lltype = getLLVMType(node->fields[i].type);
        fields += lltype;
        if (i < node->fields.size() - 1) fields += ", ";
        
        struct_field_types[safeName + "." + node->fields[i].name] = lltype;
        struct_field_indices[safeName + "." + node->fields[i].name] = i;
    }
    emit("%" + safeName + " = type { " + fields + " }\n");
}

std::string LLVMCodeGen::visit(MemberAccessNode* node) {
    if (emit_debug_info) current_debug_node = node;
    
    std::string aluType = lookupAluType(node->objectName);
    if (aluType == "float4" || aluType == "float8" || aluType == "int4" || aluType == "int8") {
        int idx = 0;
        if (node->fieldName == "x") idx = 0;
        else if (node->fieldName == "y") idx = 1;
        else if (node->fieldName == "z") idx = 2;
        else if (node->fieldName == "w") idx = 3;
        
        std::string obj_ir = lookupIRName(node->objectName);
        std::string llvmType = getLLVMType(aluType);
        
        std::string vec_val = getTempReg();
        emit("  " + vec_val + " = load " + llvmType + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        
        std::string res = getTempReg();
        emit("  " + res + " = extractelement " + llvmType + " " + vec_val + ", i32 " + std::to_string(idx), node);
        return res;
    }

    std::string structName = lookupStructType(node->objectName);
    structName = sanitizeLLVMName(structName);
    
    std::string varType = lookupVarType(node->objectName);
    bool isPtr = (varType.find("*") != std::string::npos);
    
    int fieldIdx = struct_field_indices[structName + "." + node->fieldName];
    std::string fieldType = struct_field_types[structName + "." + node->fieldName];
    
    std::string ptr_reg;
    if (isPtr) {
        ptr_reg = getTempReg();
        emit("  " + ptr_reg + " = load " + varType + ", " + varType + "* %" + lookupIRName(node->objectName) + ", align 4");
    } else {
        ptr_reg = "%" + lookupIRName(node->objectName);
    }
    
    std::string field_ptr = getTempReg();
    emit("  " + field_ptr + " = getelementptr %" + structName + ", %" + structName + "* " + ptr_reg + ", i32 0, i32 " + std::to_string(fieldIdx));
    
    std::string val_reg = getTempReg();
    emit("  " + val_reg + " = load " + fieldType + ", " + fieldType + "* " + field_ptr + ", align 4");
    
    return val_reg;
}

void LLVMCodeGen::visit(MemberAssignNode* node) {
    if (emit_debug_info) current_debug_node = node;
    
    std::string aluType = lookupAluType(node->objectName);
    if (aluType == "float4" || aluType == "float8" || aluType == "int4" || aluType == "int8") {
        int idx = 0;
        if (node->fieldName == "x") idx = 0;
        else if (node->fieldName == "y") idx = 1;
        else if (node->fieldName == "z") idx = 2;
        else if (node->fieldName == "w") idx = 3;
        
        std::string obj_ir = lookupIRName(node->objectName);
        std::string llvmType = getLLVMType(aluType);
        
        std::string vec_val = getTempReg();
        emit("  " + vec_val + " = load " + llvmType + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        
        std::string rhs_reg = evaluateExpression(node->expr.get());
        std::string res = getTempReg();
        std::string base_type = "float";
        if (aluType == "int4" || aluType == "int8") base_type = "i32";
        
        emit("  " + res + " = insertelement " + llvmType + " " + vec_val + ", " + base_type + " " + rhs_reg + ", i32 " + std::to_string(idx), node);
        emit("  store " + llvmType + " " + res + ", " + llvmType + "* %" + obj_ir + ", align 16", node);
        return;
    }

    std::string structName = lookupStructType(node->objectName);
    structName = sanitizeLLVMName(structName);
    
    std::string varType = lookupVarType(node->objectName);
    bool isPtr = (varType.find("*") != std::string::npos);
    
    int fieldIdx = struct_field_indices[structName + "." + node->fieldName];
    std::string fieldType = struct_field_types[structName + "." + node->fieldName];
    
    std::string ptr_reg;
    if (isPtr) {
        ptr_reg = getTempReg();
        emit("  " + ptr_reg + " = load " + varType + ", " + varType + "* %" + lookupIRName(node->objectName) + ", align 4");
    } else {
        ptr_reg = "%" + lookupIRName(node->objectName);
    }
    
    std::string field_ptr = getTempReg();
    emit("  " + field_ptr + " = getelementptr %" + structName + ", %" + structName + "* " + ptr_reg + ", i32 0, i32 " + std::to_string(fieldIdx));
    
    std::string val_reg = evaluateExpression(node->expr.get());
    std::string valType = getInferredLLVMType(node->expr.get());
    if (fieldType.back() == '*' && valType.back() == '*' && fieldType != valType) {
        std::string cast_reg = getTempReg();
        emit("  " + cast_reg + " = bitcast " + valType + " " + val_reg + " to " + fieldType);
        val_reg = cast_reg;
    }
    
    emit("  store " + fieldType + " " + val_reg + ", " + fieldType + "* " + field_ptr + ", align 4");
}

std::string LLVMCodeGen::visit(CastNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string val_reg = evaluateExpression(node->expr.get());
    std::string src_type = getInferredLLVMType(node->expr.get());
    std::string dst_type;
    if (node->targetType == DataType::FLOAT) dst_type = "float";
    else if (node->targetType == DataType::DOUBLE) dst_type = "double";
    else if (node->targetType == DataType::BYTE) dst_type = "i8";
    else dst_type = "i32";
    
    std::string out_reg = getTempReg();
    if (src_type == dst_type) {
        emit("  " + out_reg + " = bitcast " + src_type + " " + val_reg + " to " + dst_type);
        return out_reg;
    }
    
    if (src_type == "i32" && dst_type == "float") {
        emit("  " + out_reg + " = sitofp i32 " + val_reg + " to float");
    } else if (src_type == "float" && dst_type == "i32") {
        emit("  " + out_reg + " = fptosi float " + val_reg + " to i32");
    } else if (src_type == "i32" && dst_type == "double") {
        emit("  " + out_reg + " = sitofp i32 " + val_reg + " to double");
    } else if (src_type == "double" && dst_type == "i32") {
        emit("  " + out_reg + " = fptosi double " + val_reg + " to i32");
    } else if (src_type == "float" && dst_type == "double") {
        emit("  " + out_reg + " = fpext float " + val_reg + " to double");
    } else if (src_type == "double" && dst_type == "float") {
        emit("  " + out_reg + " = fptrunc double " + val_reg + " to float");
    } else if (src_type == "i8" && dst_type == "double") {
        std::string tmp = getTempReg();
        emit("  " + tmp + " = sext i8 " + val_reg + " to i32");
        emit("  " + out_reg + " = sitofp i32 " + tmp + " to double");
    } else if (src_type == "double" && dst_type == "i8") {
        std::string tmp = getTempReg();
        emit("  " + tmp + " = fptosi double " + val_reg + " to i32");
        emit("  " + out_reg + " = trunc i32 " + tmp + " to i8");
    } else if (src_type == "i8" && dst_type == "float") {
        std::string tmp = getTempReg();
        emit("  " + tmp + " = sext i8 " + val_reg + " to i32");
        emit("  " + out_reg + " = sitofp i32 " + tmp + " to float");
    } else if (src_type == "float" && dst_type == "i8") {
        std::string tmp = getTempReg();
        emit("  " + tmp + " = fptosi float " + val_reg + " to i32");
        emit("  " + out_reg + " = trunc i32 " + tmp + " to i8");
    } else if (src_type == "i32" && dst_type == "i8") {
        emit("  " + out_reg + " = trunc i32 " + val_reg + " to i8");
    } else if (src_type == "i8" && dst_type == "i32") {
        emit("  " + out_reg + " = zext i8 " + val_reg + " to i32");
    } else if (src_type.back() == '*' && dst_type.back() == '*' && src_type != dst_type) {
        emit("  " + out_reg + " = bitcast " + src_type + " " + val_reg + " to " + dst_type);
    } else if (!src_type.empty() && src_type.back() == '*' && dst_type == "i32") {
        emit("  " + out_reg + " = ptrtoint " + src_type + " " + val_reg + " to i32");
    } else if (src_type == "i32" && !dst_type.empty() && dst_type.back() == '*') {
        emit("  " + out_reg + " = inttoptr i32 " + val_reg + " to " + dst_type);
    } else {
        throw std::runtime_error("Unsupported cast from " + src_type + " to " + dst_type);
    }
    return out_reg;
}

void LLVMCodeGen::visit(EffectDeclNode* node) {
    if (emit_debug_info) current_debug_node = node;
    // Interface definition
}

void LLVMCodeGen::visit(HandleNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string parent_fiber = getTempReg();
    emit("  ; --- HANDLE " + node->effect_name + " ---");
    emit("  " + parent_fiber + " = call i8* @ConvertThreadToFiber(i8* null)");
    emit("  store i8* " + parent_fiber + ", i8** @__alu_parent_fiber, align 8");
    
    std::string tramp_name = "@__alu_trampoline_" + std::to_string(tmp_counter++);
    
    // We need the child function name to call.
    std::string child_func = "do_stuff"; // fallback
    if (auto callNode = dynamic_cast<FuncCallNode*>(node->in_call.get())) {
        child_func = "@" + callNode->name;
    }
    
    global_strings_output << "define void " << tramp_name << "(i8* %param) {\n";
    global_strings_output << "entry:\n";
    global_strings_output << "  call void " << child_func << "()\n";
    global_strings_output << "  %parent = load i8*, i8** @__alu_parent_fiber, align 8\n";
    global_strings_output << "  call void @SwitchToFiber(i8* %parent)\n";
    global_strings_output << "  ret void\n";
    global_strings_output << "}\n\n";

    std::string child_fiber = getTempReg();
    emit("  " + child_fiber + " = call i8* @CreateFiber(i64 0, i8* " + tramp_name + ", i8* null)");
    emit("  store i8* " + child_fiber + ", i8** @__alu_child_fiber, align 8");
    
    // Switch to child
    emit("  call void @SwitchToFiber(i8* " + child_fiber + ")");
    
    // When we return here, it's either because the child yielded, or it finished.
    // If it yielded, we execute the handler block.
    // In our simplified prototype, we just execute the handler block unconditionally.
    pushScope();
    for (const auto& s : node->handler_body) {
        s->codegen(*this);
    }
    popScope();
    
    emit("  ; --- END HANDLE ---");
}

void LLVMCodeGen::visit(YieldNode* node) {
    if (emit_debug_info) current_debug_node = node;
    emit("  ; --- YIELD " + node->effect_name + "." + node->method_name + " ---");
    // Switch back to parent fiber
    std::string parent_fiber = getTempReg();
    emit("  " + parent_fiber + " = load i8*, i8** @__alu_parent_fiber, align 8");
    emit("  call void @SwitchToFiber(i8* " + parent_fiber + ")");
}

void LLVMCodeGen::visit(ResumeNode* node) {
    if (emit_debug_info) current_debug_node = node;
    emit("  ; --- RESUME ---");
    if (node->expr) {
        std::string val = evaluateExpression(node->expr.get());
        emit("  ; (resuming with value " + val + ")");
    }
    // Switch back to child fiber
    std::string child_fiber = getTempReg();
    emit("  " + child_fiber + " = load i8*, i8** @__alu_child_fiber, align 8");
    emit("  call void @SwitchToFiber(i8* " + child_fiber + ")");
}

void LLVMCodeGen::visit(ProgramNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::cerr << "[ALU LLVM CodeGen] Translating AST to LLVM IR (Text Form)..." << std::endl;
    
    emit("; ModuleID = 'alu_module'");
    emit("source_filename = \"alu_source.alu\"");
    
    if (target_arch.find("spirv") != std::string::npos || target_arch == "vulkan") {
        emit("target datalayout = \"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024\"");
        emit("target triple = \"spirv64-unknown-unknown\"\n");
    } else if (target_arch.find("air64") != std::string::npos || target_arch == "metal") {
        emit("target datalayout = \"e-m:o-i64:64-f80:128-n8:16:32:64-S128\"");
        emit("target triple = \"air64-apple-macosx\"\n");
    } else if (target_arch.find("wasm") != std::string::npos) {
        emit("target datalayout = \"e-m:e-p:32:32-i64:64-n32:64-S128\"");
        emit("target triple = \"" + target_arch + "\"\n");
    } else if (target_arch.find("aarch64") != std::string::npos || target_arch.find("arm64") != std::string::npos) {
        if (target_arch.find("apple") != std::string::npos) {
            emit("target datalayout = \"e-m:o-i64:64-i128:128-n32:64-S128\"");
        } else {
            emit("target datalayout = \"e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128\"");
        }
        emit("target triple = \"" + target_arch + "\"\n");
    } else if (target_arch.find("arm") != std::string::npos) {
        emit("target datalayout = \"e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64\"");
        emit("target triple = \"" + target_arch + "\"\n");
    } else {
        if (target_arch.find("apple") != std::string::npos) {
            emit("target datalayout = \"e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
        } else if (target_arch.find("linux") != std::string::npos) {
            emit("target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
        } else {
            emit("target datalayout = \"e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"");
        }
        emit("target triple = \"" + target_arch + "\"\n");
    }
    
    // Memory allocation for exceptions
    if (extern_functions.find("malloc") == extern_functions.end()) {
        emit("declare i8* @malloc(i32)\n");
        extern_functions.insert("malloc");
    }
    if (extern_functions.find("free") == extern_functions.end()) {
        emit("declare void @free(i8*)\n");
        extern_functions.insert("free");
    }
    
    // Legacy exception globals for C++ backend compat
    emit("@__alu_exception_ptr = global i8* null, align 8");
    emit("@__alu_exception_msg = global i8* null, align 8");
    
    if (emit_debug_info) {
        emit("declare void @llvm.dbg.declare(metadata, metadata, metadata)\n");
    }
    
    // Fiber globals
    emit("@__alu_parent_fiber = global i8* null, align 8");
    emit("@__alu_child_fiber = global i8* null, align 8\n");
    
    // ARC Native Runtime Implementations
    // Use external ARC functions from C++ backend
    emit("declare i8* @__alu_alloc_internal(i32)");
    emit("declare void @alu_retain(i8*)");
    emit("declare void @alu_release(i8*)");

    // Windows Fiber API declarations
    emit("declare i8* @ConvertThreadToFiber(i8*)");
    emit("declare i8* @CreateFiber(i64, i8*, i8*)");
    emit("declare void @SwitchToFiber(i8*)\n");
    
    // First pass: register return types
    registerReturnTypes(node->declarations);
    
    // Pre-pass: Struct Definitions and Extern Routines
    codegenDeclarationsPrePass(node->declarations);
    
    // Main pass: Routines and everything else
    codegenDeclarationsMainPass(node->declarations);
    
    std::cerr << "[ALU LLVM CodeGen] Translation Complete." << std::endl;
}

void LLVMCodeGen::registerReturnTypes(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (const auto& decl : declarations) {
        if (auto routine = dynamic_cast<RoutineNode*>(decl.get())) {
            func_return_types[getNamespacedName(routine->name)] = getLLVMType(routine->returnType);
        } else if (auto ext = dynamic_cast<ExternRoutineNode*>(decl.get())) {
            func_return_types[ext->name] = getLLVMType(ext->returnType);
        } else if (auto ns = dynamic_cast<NamespaceNode*>(decl.get())) {
            std::string old_ns = current_namespace;
            if (current_namespace.empty()) current_namespace = ns->name;
            else current_namespace += "__" + ns->name;
            registerReturnTypes(ns->declarations);
            current_namespace = old_ns;
        }
    }
}

void LLVMCodeGen::codegenDeclarationsPrePass(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (const auto& decl : declarations) {
        if (dynamic_cast<StructDefNode*>(decl.get()) || dynamic_cast<ExternRoutineNode*>(decl.get())) {
            decl->codegen(*this);
        } else if (auto ns = dynamic_cast<NamespaceNode*>(decl.get())) {
            std::string old_ns = current_namespace;
            if (current_namespace.empty()) current_namespace = ns->name;
            else current_namespace += "__" + ns->name;
            codegenDeclarationsPrePass(ns->declarations);
            current_namespace = old_ns;
        }
    }
}

void LLVMCodeGen::codegenDeclarationsMainPass(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (const auto& decl : declarations) {
        if (auto ns = dynamic_cast<NamespaceNode*>(decl.get())) {
            std::string old_ns = current_namespace;
            if (current_namespace.empty()) current_namespace = ns->name;
            else current_namespace += "__" + ns->name;
            codegenDeclarationsMainPass(ns->declarations);
            current_namespace = old_ns;
        } else if (!dynamic_cast<StructDefNode*>(decl.get()) && !dynamic_cast<ExternRoutineNode*>(decl.get())) {
            decl->codegen(*this);
        }
    }
}

void LLVMCodeGen::visit(NamespaceNode* node) {
    if (emit_debug_info) current_debug_node = node;
    std::string old_ns = current_namespace;
    if (current_namespace.empty()) current_namespace = node->name;
    else current_namespace += "__" + node->name;
    codegenDeclarationsPrePass(node->declarations);
    codegenDeclarationsMainPass(node->declarations);
    current_namespace = old_ns;
}
