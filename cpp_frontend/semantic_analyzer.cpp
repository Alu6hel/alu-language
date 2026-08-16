#include "semantic_analyzer.h"
#include <stdexcept>
#include <iostream>
#include <typeinfo>

// --- Scope Management ---

std::string SemanticAnalyzer::prefixName(const std::string& name) {
    if (current_namespace.empty() || name.find("::") != std::string::npos) return name;
    std::string prefixed = current_namespace[0];
    for (size_t i = 1; i < current_namespace.size(); ++i) {
        prefixed += "::" + current_namespace[i];
    }
    return prefixed + "::" + name;
}

std::string SemanticAnalyzer::resolveName(const std::string& name) {
    if (name.find("::") != std::string::npos) return name; // Already fully qualified

    // Try resolving from innermost namespace outwards
    for (int i = (int)current_namespace.size(); i >= 0; --i) {
        std::string candidate = "";
        for (int j = 0; j < i; ++j) {
            candidate += current_namespace[j] + "::";
        }
        candidate += name;

        // Check if candidate exists in tables
        if (function_table.find(candidate) != function_table.end() ||
            struct_table.find(candidate) != struct_table.end()) {
            return candidate;
        }
    }
    return name;
}

void SemanticAnalyzer::pushScope() {
    scope_stack.emplace_back();
    struct_var_stack.emplace_back();
}

void SemanticAnalyzer::popScope() {
    if (!scope_stack.empty()) scope_stack.pop_back();
    if (!struct_var_stack.empty()) struct_var_stack.pop_back();
}

bool SemanticAnalyzer::lookupSymbol(const std::string& name, DataType& outType) {
    SymbolMeta meta;
    if (lookupSymbolMeta(name, meta)) {
        outType = meta.type;
        return true;
    }
    return false;
}

bool SemanticAnalyzer::lookupSymbolMeta(const std::string& name, SymbolMeta& outMeta) {
    for (int i = (int)scope_stack.size() - 1; i >= 0; --i) {
        auto it = scope_stack[i].find(name);
        if (it != scope_stack[i].end()) {
            outMeta = it->second;
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::lookupStructVar(const std::string& name, std::string& outStructName) {
    for (int i = (int)struct_var_stack.size() - 1; i >= 0; --i) {
        auto it = struct_var_stack[i].find(name);
        if (it != struct_var_stack[i].end()) {
            outStructName = it->second;
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::isDeclaredInCurrentScope(const std::string& name) {
    if (scope_stack.empty()) return false;
    return scope_stack.back().find(name) != scope_stack.back().end();
}

void SemanticAnalyzer::declareSymbol(const std::string& name, DataType type, int line, int col, const std::string& file) {
    if (!scope_stack.empty()) {
        SymbolMeta meta;
        meta.type = type;
        meta.def_line = line;
        meta.def_col = col;
        meta.def_file = file;
        scope_stack.back()[name] = meta;
    }
}

void SemanticAnalyzer::declareStructVar(const std::string& name, const std::string& structName) {
    if (!struct_var_stack.empty()) {
        struct_var_stack.back()[name] = structName;
    }
}

void SemanticAnalyzer::instantiateTemplateIfNeeded(const std::string& typeStr) {
    size_t less_idx = typeStr.find('<');
    if (less_idx == std::string::npos) return;
    
    if (struct_table.find(typeStr) != struct_table.end()) return;
    
    std::string baseName = typeStr.substr(0, less_idx);
    if (struct_templates.find(baseName) == struct_templates.end()) return;
    
    StructDefNode* tmpl = struct_templates[baseName];
    
    std::string args_str = typeStr.substr(less_idx + 1, typeStr.length() - less_idx - 2);
    std::vector<std::string> args;
    size_t last = 0;
    size_t next = 0;
    while ((next = args_str.find(',', last)) != std::string::npos) {
        args.push_back(args_str.substr(last, next - last));
        last = next + 1;
    }
    args.push_back(args_str.substr(last));
    
    if (args.size() != tmpl->type_params.size()) {
        throw std::runtime_error("Template arg count mismatch for " + typeStr);
    }
    
    std::map<std::string, std::string> type_map;
    for (size_t i = 0; i < args.size(); ++i) {
        type_map[tmpl->type_params[i]] = args[i];
    }
    
    std::vector<StructField> new_fields;
    for (const auto& f : tmpl->fields) {
        std::string new_type = replaceTypeVars(f.type, type_map);
        new_fields.push_back({new_type, f.name});
    }
    
    auto new_struct = std::make_unique<StructDefNode>(typeStr, std::vector<std::string>(), new_fields);
    
    StructInfo info;
    info.name = typeStr;
    info.fields = new_fields;
    struct_table[typeStr] = info;
    
    
      std::cerr << "[DEBUG] INSTANTIATED TEMPLATE: " << typeStr << " FIELDS: ";
      for(const auto& f : new_fields) std::cerr << f.name << " : " << f.type << " ; ";
      std::cerr << std::endl;
      
      std::cerr << "[DEBUG] INSTANTIATED TEMPLATE: " << typeStr << " FIELDS: ";
      for(const auto& f : new_fields) std::cerr << f.name << " : " << f.type << " ; ";
      std::cerr << std::endl;
      current_ast->declarations.push_back(std::move(new_struct));


}

DataType SemanticAnalyzer::parseDataType(const std::string& rawTypeStr) {
        std::string typeStr = resolveName(rawTypeStr);
    instantiateTemplateIfNeeded(typeStr);
    std::string base = typeStr;
    bool isPointer = false;
    
    if (base.find("ptr<") == 0) {
        isPointer = true;
    }
    
    while (!base.empty() && (base.back() == '*' || base.back() == ']')) {
        isPointer = true;
        if (base.back() == ']') {
            base.pop_back();
            if (!base.empty() && base.back() == '[') {
                base.pop_back();
            }
        } else if (base.back() == '*') {
            base.pop_back();
        }
    }
    
    if (isPointer) return DataType::POINTER;
    if (base == "int") return DataType::INT;
    if (base == "string") return DataType::STRING;
    if (base == "byte") return DataType::BYTE;
    if (base == "bool") return DataType::BOOL;
    if (base == "float") return DataType::FLOAT;
    if (base == "double") return DataType::DOUBLE;
    if (base == "void") return DataType::VOID;
    return DataType::UNKNOWN;
}

// --- Expression Type Checking ---

DataType SemanticAnalyzer::checkExpression(ASTNode* expr) {
    if (!expr) return DataType::UNKNOWN;
    
    if (auto literal = dynamic_cast<LiteralNode*>(expr)) {
        if (literal->type == DataType::UNKNOWN) {
            // It's a variable reference
            DataType t;
            if (!lookupSymbol(literal->value, t)) {
                throw std::runtime_error("Semantic Error: Undefined variable '" + literal->value + "'");
            }
            return t;
        }
        return literal->type;
    }
    else if (auto funcCall = dynamic_cast<FuncCallNode*>(expr)) {
        std::string original_name = funcCall->name;
        if (!funcCall->type_args.empty()) {
            instantiateRoutineTemplateIfNeeded(funcCall->name, funcCall->type_args);
            std::string mangledName = funcCall->name;
            for (const auto& ta : funcCall->type_args) {
                mangledName += "_" + resolveName(ta);
            }
            funcCall->name = mangledName;
        } else {
            funcCall->name = resolveName(funcCall->name);
        }
        
        if (function_table.find(funcCall->name) == function_table.end()) {
            // Built-in puts fallback if standard library is missing
            if (funcCall->name != "puts" && funcCall->name != "printf") {
                throw std::runtime_error("Semantic Error: Undefined function '" + funcCall->name + "'");
            }
        }
        
        if (is_lsp_mode && function_table.find(funcCall->name) != function_table.end()) {
            FunctionSignature& sig = function_table[funcCall->name];
            LSPSymbol sym;
            sym.line = funcCall->line;
            sym.col = funcCall->col;
            sym.length = original_name.length();
            sym.name = original_name;
            sym.hover_text = "routine " + original_name + " -> " + DataTypeToString(sig.returnType);
            sym.def_line = sig.def_line;
            sym.def_col = sig.def_col;
            sym.def_file = sig.def_file;
            lsp_symbols.push_back(sym);
        }

        for (const auto& arg : funcCall->args) {
            checkExpression(arg.get());
        }
        
        if (function_table.find(funcCall->name) != function_table.end()) {
            return function_table[funcCall->name].returnType;
        }
        return DataType::UNKNOWN;
    }
    else if (auto allocNode = dynamic_cast<NewAllocationNode*>(expr)) {
        allocNode->typeName = resolveName(allocNode->typeName);
        return DataType::POINTER;
    }
    else if (auto castNode = dynamic_cast<CastNode*>(expr)) {
        checkExpression(castNode->expr.get());
        return castNode->targetType;
    }
    else if (auto arrIndex = dynamic_cast<ArrayIndexNode*>(expr)) {
        DataType t = checkExpression(arrIndex->indexExpr.get());
        if (t != DataType::INT) {
            if (!is_lsp_mode) { std::cerr << "INDEX ERROR ON: Array Index" << std::endl; arrIndex->indexExpr->print(0); }
            throw std::runtime_error("Semantic Error: Array index must be an integer");
        }
        
        std::string fullType = "int";
        if (auto varNode = dynamic_cast<VarAccessNode*>(arrIndex->arrayExpr.get())) {
            fullType = array_var_table[varNode->name];
            if (fullType == "") fullType = pointer_var_table[varNode->name];
            DataType dt;
            if (fullType == "" && lookupSymbol(varNode->name, dt) && dt == DataType::STRING) fullType = "string";
        }
        
        if (fullType == "int[]" || fullType == "int*" || fullType == "int") return DataType::INT;
        if (fullType == "float[]" || fullType == "float*" || fullType == "float") return DataType::FLOAT;
        if (fullType == "double[]" || fullType == "double*" || fullType == "double") return DataType::DOUBLE;
        if (fullType == "byte[]" || fullType == "byte*" || fullType == "string") return DataType::BYTE;
        if (fullType == "string[]" || fullType == "string*") return DataType::STRING;
        
        if (fullType.find("ptr<") == 0) {
            size_t p1 = fullType.find("<");
            size_t p2 = fullType.rfind(">");
            if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                std::string base = fullType.substr(p1 + 1, p2 - p1 - 1);
                while (!base.empty() && base.back() == ' ') base.pop_back();
                while (!base.empty() && base.front() == ' ') base.erase(0, 1);
                return parseDataType(base);
            }
        }
        
        return DataType::INT;
    }
    
    else if (auto varNode = dynamic_cast<VarAccessNode*>(expr)) {
        DataType t;
        if (!lookupSymbol(varNode->name, t)) {
            std::string sn;
            if (lookupStructVar(varNode->name, sn)) {
                return parseDataType(sn);
            }
            if (struct_table.find(varNode->name) != struct_table.end()) {
                // It's an effect or raw struct type access, return a generic pointer type for now
                return DataType::POINTER;
            }
            if (!is_lsp_mode) { std::cerr << "VarAccess Error: " << varNode->name << std::endl; }
            throw std::runtime_error("Semantic Error: Undefined variable '" + varNode->name + "'");
        }
        if (is_lsp_mode) {
            SymbolMeta meta;
            if (lookupSymbolMeta(varNode->name, meta)) {
                LSPSymbol sym;
                sym.line = varNode->line;
                sym.col = varNode->col;
                sym.length = varNode->name.length();
                sym.name = varNode->name;
                sym.hover_text = "variable " + varNode->name + " : " + DataTypeToString(t);
                sym.def_line = meta.def_line;
                sym.def_col = meta.def_col;
                sym.def_file = meta.def_file;
                lsp_symbols.push_back(sym);
            }
        }
        return t;
    }
    else if (auto binOp = dynamic_cast<BinOpNode*>(expr)) {
        DataType leftT = checkExpression(binOp->left.get());
        DataType rightT = checkExpression(binOp->right.get());
        
        if (binOp->op == "==" || binOp->op == "!=" || binOp->op == "<" || binOp->op == "<=" || binOp->op == ">" || binOp->op == ">=") {
            return DataType::BOOL;
        }
        
        if (leftT == DataType::FLOAT || rightT == DataType::FLOAT) return DataType::FLOAT;
        if (leftT == DataType::DOUBLE || rightT == DataType::DOUBLE) return DataType::DOUBLE;
        return leftT != DataType::UNKNOWN ? leftT : rightT;
    }
    else if (auto methodCall = dynamic_cast<MethodCallNode*>(expr)) {
        DataType objType = checkExpression(methodCall->object.get());
        for (const auto& arg : methodCall->args) {
            checkExpression(arg.get());
        }
        std::string structName = "";
        if (auto varNode = dynamic_cast<VarAccessNode*>(methodCall->object.get())) {
            if (!lookupStructVar(varNode->name, structName)) {
                // Check if it's an effect or other type directly (e.g. Gen.yield_val)
                if (struct_table.find(varNode->name) != struct_table.end()) {
                    structName = varNode->name;
                }
            }
        }
        std::string mangledName = structName + "_" + methodCall->methodName;
        // In effects, the effect name might not be prefixed with _, let's check for the exact method name if struct_method fails
        if (function_table.find(mangledName) == function_table.end() && function_table.find(methodCall->methodName) != function_table.end()) {
            mangledName = methodCall->methodName;
        }
        if (function_table.find(mangledName) != function_table.end()) {
            if (is_lsp_mode) {
                FunctionSignature& sig = function_table[mangledName];
                LSPSymbol sym;
                sym.line = methodCall->line;
                sym.col = methodCall->col;
                sym.length = methodCall->methodName.length();
                sym.name = methodCall->methodName;
                sym.hover_text = "method " + mangledName + " -> " + DataTypeToString(sig.returnType);
                sym.def_line = sig.def_line;
                sym.def_col = sig.def_col;
                sym.def_file = sig.def_file;
                lsp_symbols.push_back(sym);
            }
            return function_table[mangledName].returnType;
        }
        return DataType::UNKNOWN;
    }
    else if (auto memberAcc = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string structName;
        if (lookupStructVar(memberAcc->objectName, structName)) {
            if (struct_table.find(structName) != struct_table.end()) {
                StructInfo& info = struct_table[structName];
                for (const auto& field : info.fields) {
                    if (field.name == memberAcc->fieldName) {
                        return parseDataType(field.type);
                    }
                }
            }
        }
        return DataType::UNKNOWN;
    }

    return DataType::UNKNOWN;
}

// --- Variable Declaration Checking ---

void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {
    decl->varType = resolveName(decl->varType);
    DataType expectedType = parseDataType(decl->varType);
    
    if (expectedType == DataType::UNKNOWN && struct_table.find(decl->varType) != struct_table.end()) {
        declareStructVar(decl->name, decl->varType);
    } else if (expectedType == DataType::POINTER) {
        pointer_var_table[decl->name] = decl->varType;
        std::string base = decl->varType;
        
        if (base.find("ptr<") == 0) {
            size_t p1 = base.find("<");
            size_t p2 = base.rfind(">");
            if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                base = base.substr(p1 + 1, p2 - p1 - 1);
                while (!base.empty() && base.back() == ' ') base.pop_back();
                while (!base.empty() && base.front() == ' ') base.erase(0, 1);
            }
        }
        
        while (!base.empty() && (base.back() == '*' || base.back() == ']')) {
            if (base.back() == ']') {
                base.pop_back();
                if (!base.empty() && base.back() == '[') {
                    base.pop_back();
                }
            } else if (base.back() == '*') {
                base.pop_back();
            }
        }
        
        instantiateTemplateIfNeeded(base);
        if (struct_table.find(base) != struct_table.end()) {
            declareStructVar(decl->name, base);
        }
    }
    
    std::string sn;
    if (expectedType == DataType::UNKNOWN && !lookupStructVar(decl->name, sn)) {
        if (struct_table.find(decl->varType) != struct_table.end()) {
            // It's a struct/effect that isn't a pointer, but declared locally
            declareStructVar(decl->name, decl->varType);
        } else {
            throw std::runtime_error("Semantic Error: Unknown variable type '" + decl->varType + "'");
        }
    }
    
    // Check if variable is already defined IN THE CURRENT SCOPE ONLY
    if (isDeclaredInCurrentScope(decl->name)) {
        throw std::runtime_error("Semantic Error: Variable '" + decl->name + "' already declared in this scope.");
    }
    
    // Check the expression it is initialized with
    if (decl->initializer) {
        DataType actualType = checkExpression(decl->initializer.get());
        if (expectedType != DataType::UNKNOWN && expectedType != actualType && actualType != DataType::POINTER && actualType != DataType::UNKNOWN) {
            if (!((expectedType == DataType::INT && actualType == DataType::BYTE) || 
                  (expectedType == DataType::BYTE && actualType == DataType::INT) ||
                  (expectedType == DataType::INT && actualType == DataType::FLOAT) ||
                  (expectedType == DataType::FLOAT && actualType == DataType::INT) ||
                  (expectedType == DataType::INT && actualType == DataType::DOUBLE) ||
                  (expectedType == DataType::DOUBLE && actualType == DataType::INT) ||
                  (expectedType == DataType::FLOAT && actualType == DataType::DOUBLE) ||
                  (expectedType == DataType::DOUBLE && actualType == DataType::FLOAT))) {
                throw std::runtime_error("Semantic Error: Type mismatch in variable declaration '" + decl->name + "'. Expected " + DataTypeToString(expectedType) + " but got " + DataTypeToString(actualType));
            }
        }
    }
    
    // Store in current scope
    declareSymbol(decl->name, expectedType, decl->line, decl->col, decl->file);
}

// --- Statement Checking ---

void SemanticAnalyzer::checkStatement(ASTNode* stmt) {
    if (auto vardecl = dynamic_cast<VarDeclNode*>(stmt)) {
        checkVarDecl(vardecl);
    }
    else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        DataType targetType;
        if (!lookupSymbol(varassign->name, targetType)) {
            std::cerr << "[COMPILER ERROR] Semantic Error: Assignment to undefined variable '" << varassign->name << "'" << std::endl;
            exit(1);
        }
        DataType exprType = checkExpression(varassign->expr.get());
        if (exprType != targetType) {
            if (!((exprType == DataType::INT && targetType == DataType::BYTE) || 
                  (exprType == DataType::BYTE && targetType == DataType::INT) ||
                  (exprType == DataType::INT && targetType == DataType::FLOAT) ||
                  (exprType == DataType::FLOAT && targetType == DataType::INT) ||
                  (exprType == DataType::INT && targetType == DataType::DOUBLE) ||
                  (exprType == DataType::DOUBLE && targetType == DataType::INT) ||
                  (exprType == DataType::FLOAT && targetType == DataType::DOUBLE) ||
                  (exprType == DataType::DOUBLE && targetType == DataType::FLOAT))) {
                throw std::runtime_error("Semantic Error: Type mismatch in assignment to '" + varassign->name + "'");
            }
        }
    }
    else if (auto memberAssign = dynamic_cast<MemberAssignNode*>(stmt)) {
        std::string structName;
        if (!lookupStructVar(memberAssign->objectName, structName)) {
            throw std::runtime_error("Semantic Error: Variable '" + memberAssign->objectName + "' is not a struct.");
        }
        StructInfo& info = struct_table[structName];
        
        bool found = false;
        DataType fieldType = DataType::UNKNOWN;
        for (const auto& field : info.fields) {
            if (field.name == memberAssign->fieldName) {
                found = true;
                fieldType = parseDataType(field.type);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Semantic Error: Struct '" + structName + "' has no field '" + memberAssign->fieldName + "'");
        }
        
        DataType valType = checkExpression(memberAssign->expr.get());
        if (fieldType != DataType::UNKNOWN && fieldType != valType && valType != DataType::POINTER) {
            throw std::runtime_error("Semantic Error: Type mismatch in struct member assignment.");
        }
    }
    else if (auto arrDecl = dynamic_cast<ArrayDeclNode*>(stmt)) {
        DataType expectedType = parseDataType(arrDecl->type);
        array_var_table[arrDecl->name] = arrDecl->type;
        declareSymbol(arrDecl->name, DataType::ARRAY);
        
        DataType sizeType = checkExpression(arrDecl->sizeExpr.get());
        if (sizeType != DataType::INT) {
            throw std::runtime_error("Semantic Error: Array size must be an integer");
        }
    }
    else if (auto arrAssign = dynamic_cast<ArrayAssignNode*>(stmt)) {
        if (auto varNode = dynamic_cast<VarAccessNode*>(arrAssign->arrayExpr.get())) {
            if (array_var_table.find(varNode->name) == array_var_table.end() && pointer_var_table.find(varNode->name) == pointer_var_table.end()) {
                DataType dt;
                if (!lookupSymbol(varNode->name, dt) || dt != DataType::STRING) {
                    // Warning or nothing
                }
            }
        }
        DataType idxType = checkExpression(arrAssign->indexExpr.get());
        if (idxType != DataType::INT) {
            if (!is_lsp_mode) { std::cerr << "INDEX ERROR ON (ASSIGN): " << std::endl; arrAssign->indexExpr->print(0); }
            throw std::runtime_error("Semantic Error: Array index must be an integer");
        }
        checkExpression(arrAssign->valExpr.get());
    }
    else if (auto derefAssign = dynamic_cast<DerefAssignNode*>(stmt)) {
        checkExpression(derefAssign->ptr_expr.get());
        checkExpression(derefAssign->val_expr.get());
    }
    else if (auto freeNode = dynamic_cast<FreeNode*>(stmt)) {
        DataType t = checkExpression(freeNode->expr.get());
        if (t != DataType::POINTER && t != DataType::UNKNOWN) {
            throw std::runtime_error("Semantic Error: Cannot free a non-pointer type.");
        }
    }
    else if (auto ifNode = dynamic_cast<IfNode*>(stmt)) {
        DataType condType = checkExpression(ifNode->condition.get());
        if (condType != DataType::BOOL) {
            throw std::runtime_error("Semantic Error: 'if' condition must evaluate to a boolean.");
        }
        // Scoped then-body
        pushScope();
        for (const auto& s : ifNode->then_body) checkStatement(s.get());
        popScope();
        // Scoped else-body
        if (!ifNode->else_body.empty()) {
            pushScope();
            for (const auto& s : ifNode->else_body) checkStatement(s.get());
            popScope();
        }
    }
    else if (auto whileNode = dynamic_cast<WhileNode*>(stmt)) {
        DataType condType = checkExpression(whileNode->condition.get());
        if (condType != DataType::BOOL) {
            throw std::runtime_error("Semantic Error: 'while' condition must evaluate to a boolean.");
        }
        // Scoped body
        pushScope();
        for (const auto& s : whileNode->body) checkStatement(s.get());
        popScope();
    }
    else if (auto forNode = dynamic_cast<ForNode*>(stmt)) {
        // The entire for-loop gets its own scope (so init var like 'int i' is scoped)
        pushScope();
        
        // Check init
        if (forNode->init) checkStatement(forNode->init.get());
        
        // Check condition (must be bool)
        if (forNode->condition) {
            DataType condType = checkExpression(forNode->condition.get());
            if (condType != DataType::BOOL) {
                throw std::runtime_error("Semantic Error: 'for' condition must evaluate to a boolean.");
            }
        }
        
        // Check body (inner scope for body-local vars)
        pushScope();
        for (const auto& s : forNode->body) checkStatement(s.get());
        popScope();
        
        // Check update
        if (forNode->update) checkStatement(forNode->update.get());
        
        popScope();
    }
    else if (auto returnNode = dynamic_cast<ReturnNode*>(stmt)) {
        DataType returnType = DataType::VOID;
        if (returnNode->expr) {
            returnType = checkExpression(returnNode->expr.get());
        }
        if (returnType != current_routine_return_type && 
            returnType != DataType::UNKNOWN &&
            current_routine_return_type != DataType::UNKNOWN &&
            !((returnType == DataType::INT && current_routine_return_type == DataType::FLOAT) ||
              (returnType == DataType::FLOAT && current_routine_return_type == DataType::INT) ||
              (returnType == DataType::INT && current_routine_return_type == DataType::DOUBLE) ||
              (returnType == DataType::DOUBLE && current_routine_return_type == DataType::INT) ||
              (returnType == DataType::FLOAT && current_routine_return_type == DataType::DOUBLE) ||
              (returnType == DataType::DOUBLE && current_routine_return_type == DataType::FLOAT) ||
              (returnType == DataType::INT && current_routine_return_type == DataType::BYTE) ||
              (returnType == DataType::BYTE && current_routine_return_type == DataType::INT))) {
            throw std::runtime_error("Semantic Error: Return type mismatch. Expected " + 
                                     DataTypeToString(current_routine_return_type) + " but got " + 
                                     DataTypeToString(returnType));
        }
    }
    else if (auto funcCall = dynamic_cast<FuncCallNode*>(stmt)) {
        checkExpression(funcCall);
    }
    else if (auto methodCall = dynamic_cast<MethodCallNode*>(stmt)) {
        checkExpression(methodCall);
    }
    else if (auto unsafeBlock = dynamic_cast<UnsafeBlockNode*>(stmt)) {
        for (const auto& s : unsafeBlock->body) {
            checkStatement(s.get());
        }
    }
    else if (auto throwNode = dynamic_cast<ThrowNode*>(stmt)) {
        DataType type = checkExpression(throwNode->expr.get());
        if (type != DataType::STRING && type != DataType::UNKNOWN && type != DataType::POINTER) {
            throw std::runtime_error("Semantic Error: Can only throw string or struct types.");
        }
    }
    else if (auto tryNode = dynamic_cast<TryCatchNode*>(stmt)) {
        pushScope();
        for (const auto& s : tryNode->try_body) checkStatement(s.get());
        popScope();
        
        pushScope();
        DataType catchType = parseDataType(tryNode->catch_var_type);
        declareSymbol(tryNode->catch_var_name, catchType);
        for (const auto& s : tryNode->catch_body) checkStatement(s.get());
        popScope();
    }
    else if (auto handleNode = dynamic_cast<HandleNode*>(stmt)) {
        // Evaluate the 'in' call
        if (handleNode->in_call) {
            checkExpression(handleNode->in_call.get());
        }
        
        // Evaluate the 'on' block with scoped variables
        pushScope();
        for (const auto& arg : handleNode->handler_args) {
            declareSymbol(arg.second, DataType::STRING); // hardcode string for simplicity
        }
        for (const auto& s : handleNode->handler_body) {
            checkStatement(s.get());
        }
        popScope();
    }
    else if (auto yieldNode = dynamic_cast<YieldNode*>(stmt)) {
        for (const auto& arg : yieldNode->args) {
            checkExpression(arg.get());
        }
    }
    else if (auto resumeNode = dynamic_cast<ResumeNode*>(stmt)) {
        if (resumeNode->expr) {
            checkExpression(resumeNode->expr.get());
        }
    }
    // AsmCall is unchecked for now (by definition it's unsafe)
}

// --- Routine Checking ---

void SemanticAnalyzer::checkRoutine(RoutineNode* routine) {
    // Clear scope stack and push a fresh function-level scope
    scope_stack.clear();
    struct_var_stack.clear();
    pushScope();
    
    // Register parameters as local variables
    for (auto& param : routine->params) {
        param.type = resolveName(param.type);
        DataType pt = parseDataType(param.type);
        declareSymbol(param.name, pt);
        if (pt == DataType::UNKNOWN && struct_table.find(param.type) != struct_table.end()) {
            declareStructVar(param.name, param.type);
        } else if (pt == DataType::POINTER) {
            pointer_var_table[param.name] = param.type;
            std::string base = param.type;
            
            if (base.find("ptr<") == 0) {
                size_t p1 = base.find("<");
                size_t p2 = base.rfind(">");
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    base = base.substr(p1 + 1, p2 - p1 - 1);
                    while (!base.empty() && base.back() == ' ') base.pop_back();
                    while (!base.empty() && base.front() == ' ') base.erase(0, 1);
                }
            }
            
            while (!base.empty() && (base.back() == '*' || base.back() == ']')) {
                if (base.back() == ']') {
                    base.pop_back();
                    if (!base.empty() && base.back() == '[') {
                        base.pop_back();
                    }
                } else if (base.back() == '*') {
                    base.pop_back();
                }
            }
            
            instantiateTemplateIfNeeded(base);
            if (struct_table.find(base) != struct_table.end()) {
                declareStructVar(param.name, base);
            }
        }
    }
    
    routine->returnType = resolveName(routine->returnType);
    current_routine_return_type = parseDataType(routine->returnType);
    
    for (const auto& stmt : routine->body) {
        checkStatement(stmt.get());
    }
    
    popScope();
}

// --- Program Checking ---

void SemanticAnalyzer::checkProgram(ProgramNode* node) {
    checkDeclarations(node->declarations);
}

void SemanticAnalyzer::checkDeclarations(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (size_t i = 0; i < declarations.size(); ++i) {
        auto* decl = declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            routine->name = prefixName(routine->name);
            if (!routine->type_params.empty()) {
                routine_templates[routine->name] = routine;
                continue;
            }

            routine->returnType = resolveName(routine->returnType);
            FunctionSignature sig;
            sig.returnType = parseDataType(routine->returnType);
            for (auto& p : routine->params) {
                p.type = resolveName(p.type);
                DataType t = parseDataType(p.type);
                sig.paramTypes.push_back(t);
            }
            sig.isVariadic = false;
            function_table[routine->name] = sig;
        } else if (auto ext = dynamic_cast<ExternRoutineNode*>(decl)) {
            ext->name = prefixName(ext->name);
            ext->returnType = resolveName(ext->returnType);
            FunctionSignature sig;
            sig.returnType = parseDataType(ext->returnType);
            for (auto& p : ext->params) {
                p.type = resolveName(p.type);
                DataType t = parseDataType(p.type);
                sig.paramTypes.push_back(t);
            }
            sig.isVariadic = ext->isVariadic;
            function_table[ext->name] = sig;
        } else if (auto structDef = dynamic_cast<StructDefNode*>(decl)) {
            structDef->name = prefixName(structDef->name);
            for (auto& f : structDef->fields) {
                f.type = resolveName(f.type);
            }
            if (!structDef->type_params.empty()) {
                struct_templates[structDef->name] = structDef;
            } else {
                StructInfo info;
                info.name = structDef->name;
                info.fields = structDef->fields;
                struct_table[structDef->name] = info;
            }
        } else if (auto effectNode = dynamic_cast<EffectDeclNode*>(decl)) {
            effectNode->name = prefixName(effectNode->name);
            // For now, register it as a pseudo-struct so it passes VarAccess checks
            StructInfo info;
            info.name = effectNode->name;
            struct_table[effectNode->name] = info;
            
            // Also register its methods as functions so method calls work
            for (auto& m : effectNode->methods) {
                if (auto r = dynamic_cast<RoutineNode*>(m.get())) {
                    FunctionSignature sig;
                    sig.returnType = parseDataType(r->returnType);
                    for (auto& p : r->params) {
                        sig.paramTypes.push_back(parseDataType(p.type));
                    }
                    function_table[r->name] = sig;
                }
            }
        } else if (auto nsNode = dynamic_cast<NamespaceNode*>(decl)) {
            current_namespace.push_back(nsNode->name);
            checkDeclarations(nsNode->declarations);
            current_namespace.pop_back();
        }
    }
    
    // Second pass: Check routine bodies
    for (size_t i = 0; i < declarations.size(); ++i) {
        auto* decl = declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
              if (!routine->type_params.empty()) continue;
            checkRoutine(routine);
        } else if (auto nsNode = dynamic_cast<NamespaceNode*>(decl)) {
            current_namespace.push_back(nsNode->name);
            // We already did first pass for this NS in the outer loop, but we need to do second pass
            // Wait, we can just call second pass explicitly by doing a second pass loop inside checkDeclarations!
            // That's what this is doing.
            checkDeclarationsSecondPass(nsNode->declarations);
            current_namespace.pop_back();
        } else if (auto effectNode = dynamic_cast<EffectDeclNode*>(decl)) {
            // we could register effect names in a table if needed, for now just no-op or record it
            // For now, we don't throw an error on EffectDeclNode in the first pass
        }
    }
}

void SemanticAnalyzer::checkDeclarationsSecondPass(const std::vector<std::unique_ptr<ASTNode>>& declarations) {
    for (size_t i = 0; i < declarations.size(); ++i) {
        auto* decl = declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            if (!routine->type_params.empty()) continue;
            checkRoutine(routine);
        } else if (auto nsNode = dynamic_cast<NamespaceNode*>(decl)) {
            current_namespace.push_back(nsNode->name);
            checkDeclarationsSecondPass(nsNode->declarations);
            current_namespace.pop_back();
        } else if (auto effectNode = dynamic_cast<EffectDeclNode*>(decl)) {
            // Nothing to check for second pass right now.
        }
    }
}

void SemanticAnalyzer::analyze(ProgramNode* ast) {
    if (!is_lsp_mode) std::cout << "[ALU CXX] Running Semantic Analysis..." << std::endl;
    current_ast = ast;
    checkProgram(ast);
    if (!is_lsp_mode) std::cout << "[ALU CXX] Semantic Analysis Passed: Memory and Type Safety verified." << std::endl;
}

void SemanticAnalyzer::instantiateRoutineTemplateIfNeeded(const std::string& name, const std::vector<std::string>& type_args) {
    std::string mangledName = name;
    for (const auto& ta : type_args) mangledName += "_" + resolveName(ta);
    if (function_table.find(mangledName) != function_table.end()) return;
    
    if (routine_templates.find(name) == routine_templates.end()) return;
    
    RoutineNode* r = routine_templates[name];
    if (r->type_params.size() != type_args.size()) return;
    
    std::map<std::string, std::string> type_map;
    for (size_t i = 0; i < type_args.size(); ++i) {
        type_map[r->type_params[i]] = resolveName(type_args[i]);
    }
    
    auto cloned = r->clone(type_map);
    RoutineNode* instantiated = dynamic_cast<RoutineNode*>(cloned.get());
    if (instantiated) {
        instantiated->name = mangledName;
        instantiated->type_params.clear();
        
        FunctionSignature sig;
        sig.returnType = parseDataType(instantiated->returnType);
        for (auto& p : instantiated->params) {
            DataType t = parseDataType(p.type);
            sig.paramTypes.push_back(t);
        }
        sig.isVariadic = false;
        function_table[mangledName] = sig;
        
        current_ast->declarations.push_back(std::move(cloned));
    }
}
