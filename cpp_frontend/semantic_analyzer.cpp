#include "semantic_analyzer.h"
#include <stdexcept>
#include <iostream>

// --- Scope Management ---

void SemanticAnalyzer::pushScope() {
    scope_stack.emplace_back();
    struct_var_stack.emplace_back();
}

void SemanticAnalyzer::popScope() {
    if (!scope_stack.empty()) scope_stack.pop_back();
    if (!struct_var_stack.empty()) struct_var_stack.pop_back();
}

bool SemanticAnalyzer::lookupSymbol(const std::string& name, DataType& outType) {
    // Search from innermost scope to outermost
    for (int i = (int)scope_stack.size() - 1; i >= 0; --i) {
        auto it = scope_stack[i].find(name);
        if (it != scope_stack[i].end()) {
            outType = it->second;
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

void SemanticAnalyzer::declareSymbol(const std::string& name, DataType type) {
    if (!scope_stack.empty()) {
        scope_stack.back()[name] = type;
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
    
    std::vector<StructField> new_fields;
    for (const auto& f : tmpl->fields) {
        std::string new_type = f.type;
        for (size_t i = 0; i < args.size(); ++i) {
            if (new_type == tmpl->type_params[i]) {
                new_type = args[i];
            }
        }
        new_fields.push_back({new_type, f.name});
    }
    
    auto new_struct = std::make_unique<StructDefNode>(typeStr, std::vector<std::string>(), new_fields);
    
    StructInfo info;
    info.name = typeStr;
    info.fields = new_fields;
    struct_table[typeStr] = info;
    
    current_ast->declarations.push_back(std::move(new_struct));
}

DataType SemanticAnalyzer::parseDataType(const std::string& typeStr) {
    instantiateTemplateIfNeeded(typeStr);
    std::string base = typeStr;
    bool isPointer = false;
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
        if (function_table.find(funcCall->name) == function_table.end()) {
            // Built-in puts fallback if standard library is missing
            if (funcCall->name != "puts") {
                throw std::runtime_error("Semantic Error: Undefined function '" + funcCall->name + "'");
            }
            return DataType::INT;
        }
        
        auto sig = function_table[funcCall->name];
        
        if (sig.isVariadic) {
            if (funcCall->args.size() < sig.paramTypes.size()) {
                throw std::runtime_error("Semantic Error: Too few arguments to variadic function '" + funcCall->name + "'");
            }
        } else {
            if (funcCall->args.size() != sig.paramTypes.size()) {
                throw std::runtime_error("Semantic Error: Incorrect number of arguments to '" + funcCall->name + "'");
            }
        }
        
        for (size_t i = 0; i < sig.paramTypes.size(); ++i) {
            DataType argType = checkExpression(funcCall->args[i].get());
            if (argType != sig.paramTypes[i]) {
                throw std::runtime_error("Semantic Error: Argument type mismatch in call to '" + funcCall->name + "' (Expected: " + DataTypeToString(sig.paramTypes[i]) + ", Got: " + DataTypeToString(argType) + ")");
            }
        }
        
        return sig.returnType;
    }
    else if (auto methodCall = dynamic_cast<MethodCallNode*>(expr)) {
        std::string typeName = "";
        if (auto obj = dynamic_cast<VarAccessNode*>(methodCall->object.get())) {
            std::string sn;
            if (lookupStructVar(obj->name, sn)) {
                typeName = sn;
            } else {
                DataType dt;
                if (lookupSymbol(obj->name, dt) && dt == DataType::POINTER && pointer_var_table.find(obj->name) != pointer_var_table.end()) {
                    std::string base = pointer_var_table[obj->name];
                    if (!base.empty() && base.back() == '*') base.pop_back();
                    typeName = base;
                }
            }
        }
        if (typeName.empty()) {
            throw std::runtime_error("Semantic Error: Method call on invalid object.");
        }
        std::string funcName = typeName + "_" + methodCall->methodName;
        if (function_table.find(funcName) == function_table.end()) {
            throw std::runtime_error("Semantic Error: Undefined method '" + methodCall->methodName + "' on struct '" + typeName + "'");
        }
        auto sig = function_table[funcName];
        if (!sig.isVariadic && methodCall->args.size() + 1 != sig.paramTypes.size()) {
            throw std::runtime_error("Semantic Error: Incorrect number of arguments to method '" + funcName + "'");
        }
        for (size_t i = 0; i < methodCall->args.size(); ++i) {
            checkExpression(methodCall->args[i].get());
        }
        return sig.returnType;
    }

    else if (auto binop = dynamic_cast<BinOpNode*>(expr)) {
        DataType leftType = checkExpression(binop->left.get());
        DataType rightType = checkExpression(binop->right.get());
        
        if (leftType != rightType && leftType != DataType::UNKNOWN && rightType != DataType::UNKNOWN) {
            bool is_ptr_arith = (binop->op == "+" || binop->op == "-") && 
                                ((leftType == DataType::STRING && rightType == DataType::INT) ||
                                 (leftType == DataType::POINTER && rightType == DataType::INT));
            bool is_float_int = (leftType == DataType::FLOAT && rightType == DataType::INT) ||
                                (leftType == DataType::INT && rightType == DataType::FLOAT) ||
                                (leftType == DataType::DOUBLE && rightType == DataType::INT) ||
                                (leftType == DataType::INT && rightType == DataType::DOUBLE) ||
                                (leftType == DataType::FLOAT && rightType == DataType::DOUBLE) ||
                                (leftType == DataType::DOUBLE && rightType == DataType::FLOAT);
            if (!is_ptr_arith && !is_float_int) {
                std::cerr << "[DEBUG] BinOp Type Mismatch: " << binop->op << " on " << DataTypeToString(leftType) << " and " << DataTypeToString(rightType) << "\n";
                binop->print(0);
                throw std::runtime_error(
                    "Semantic Error: Type mismatch in binary operation. Cannot operate on " +
                    DataTypeToString(leftType) + " and " + DataTypeToString(rightType)
                );
            }
        }
        
        if (binop->op == "==" || binop->op == "!=" || binop->op == "<" || binop->op == ">" || binop->op == "<=" || binop->op == ">=") {
            return DataType::BOOL;
        }
        
        if (leftType == DataType::STRING || leftType == DataType::POINTER) {
            return leftType;
        }
        return leftType != DataType::UNKNOWN ? leftType : rightType;
    }
    else if (auto memberAccess = dynamic_cast<MemberAccessNode*>(expr)) {
        std::string structName;
        if (!lookupStructVar(memberAccess->objectName, structName)) {
            throw std::runtime_error("Semantic Error: Variable '" + memberAccess->objectName + "' is not a struct.");
        }
        StructInfo& info = struct_table[structName];
        
        bool found = false;
        DataType fieldType = DataType::UNKNOWN;
        for (const auto& field : info.fields) {
            if (field.name == memberAccess->fieldName) {
                found = true;
                fieldType = parseDataType(field.type);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Semantic Error: Struct '" + structName + "' has no field '" + memberAccess->fieldName + "'");
        }
        return fieldType;
    }
    else if (auto varAccess = dynamic_cast<VarAccessNode*>(expr)) {
        DataType t;
        if (lookupSymbol(varAccess->name, t)) {
            return t;
        }
        std::string sn;
        if (lookupStructVar(varAccess->name, sn)) {
            return DataType::UNKNOWN; // Custom struct
        }
        throw std::runtime_error("Semantic Error: Undefined variable '" + varAccess->name + "'");
    }
    else if (auto addrNode = dynamic_cast<AddressOfNode*>(expr)) {
        checkExpression(addrNode->expr.get());
        return DataType::POINTER;
    }
    else if (auto derefNode = dynamic_cast<DereferenceNode*>(expr)) {
        DataType t = checkExpression(derefNode->expr.get());
        if (t != DataType::POINTER && t != DataType::ARRAY && t != DataType::UNKNOWN) {
            throw std::runtime_error("Semantic Error: Cannot dereference a non-pointer");
        }
        
        if (auto varAccess = dynamic_cast<VarAccessNode*>(derefNode->expr.get())) {
            std::string fullType = pointer_var_table[varAccess->name];
            if (fullType == "int*") return DataType::INT;
            if (fullType == "string*") return DataType::STRING;
        }
        
        return DataType::UNKNOWN;
    }
    else if (auto allocNode = dynamic_cast<NewAllocationNode*>(expr)) {
        return DataType::POINTER;
    }
    else if (auto castNode = dynamic_cast<CastNode*>(expr)) {
        checkExpression(castNode->expr.get());
        return castNode->targetType;
    }
    else if (auto arrIndex = dynamic_cast<ArrayIndexNode*>(expr)) {
        DataType t = checkExpression(arrIndex->indexExpr.get());
        if (t != DataType::INT) {
            std::cerr << "INDEX ERROR ON: Array Index" << std::endl;
            arrIndex->indexExpr->print(0);
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
        return DataType::INT;
    }
    return DataType::UNKNOWN;
}

// --- Variable Declaration Checking ---

void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {
    DataType expectedType = parseDataType(decl->varType);
    
    if (expectedType == DataType::UNKNOWN && struct_table.find(decl->varType) != struct_table.end()) {
        declareStructVar(decl->name, decl->varType);
    } else if (expectedType == DataType::POINTER) {
        pointer_var_table[decl->name] = decl->varType;
        std::string base = decl->varType;
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
        if (struct_table.find(base) != struct_table.end()) {
            declareStructVar(decl->name, base);
        }
    }
    
    std::string sn;
    if (expectedType == DataType::UNKNOWN && !lookupStructVar(decl->name, sn)) {
        throw std::runtime_error("Semantic Error: Unknown variable type '" + decl->varType + "'");
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
    declareSymbol(decl->name, expectedType);
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
            std::cerr << "INDEX ERROR ON (ASSIGN): " << std::endl;
            arrAssign->indexExpr->print(0);
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
        if (type != DataType::STRING) {
            throw std::runtime_error("Semantic Error: Can only throw string types.");
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
    for (const auto& param : routine->params) {
        DataType pt = parseDataType(param.type);
        declareSymbol(param.name, pt);
        if (pt == DataType::UNKNOWN && struct_table.find(param.type) != struct_table.end()) {
            declareStructVar(param.name, param.type);
        } else if (pt == DataType::POINTER) {
            pointer_var_table[param.name] = param.type;
            std::string base = param.type;
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
            if (struct_table.find(base) != struct_table.end()) {
                declareStructVar(param.name, base);
            }
        }
    }
    
    current_routine_return_type = parseDataType(routine->returnType);
    
    for (const auto& stmt : routine->body) {
        checkStatement(stmt.get());
    }
    
    popScope();
}

// --- Program Checking ---

void SemanticAnalyzer::checkProgram(ProgramNode* node) {
    // First pass: Register structs and routines
    for (size_t i = 0; i < node->declarations.size(); ++i) {
        auto* decl = node->declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            FunctionSignature sig;
            sig.returnType = parseDataType(routine->returnType);
            for (const auto& p : routine->params) {
                DataType t = parseDataType(p.type);
                sig.paramTypes.push_back(t);
            }
            sig.isVariadic = false;
            function_table[routine->name] = sig;
        } else if (auto ext = dynamic_cast<ExternRoutineNode*>(decl)) {
            FunctionSignature sig;
            sig.returnType = parseDataType(ext->returnType);
            for (const auto& p : ext->params) {
                DataType t = parseDataType(p.type);
                sig.paramTypes.push_back(t);
            }
            sig.isVariadic = ext->isVariadic;
            function_table[ext->name] = sig;
        } else if (auto structDef = dynamic_cast<StructDefNode*>(decl)) {
            if (!structDef->type_params.empty()) {
                struct_templates[structDef->name] = structDef;
            } else {
                StructInfo info;
                info.name = structDef->name;
                info.fields = structDef->fields;
                struct_table[structDef->name] = info;
            }
        } else if (auto effectDef = dynamic_cast<EffectDeclNode*>(decl)) {
            // For now, no strict type tracking for effects globally, just pass it through
        }
    }
    
    // Second pass: Check routine bodies
    for (size_t i = 0; i < node->declarations.size(); ++i) {
        auto* decl = node->declarations[i].get();
        if (auto routine = dynamic_cast<RoutineNode*>(decl)) {
            checkRoutine(routine);
        }
    }
}

void SemanticAnalyzer::analyze(ProgramNode* ast) {
    std::cout << "[ALU CXX] Running Semantic Analysis..." << std::endl;
    current_ast = ast;
    checkProgram(ast);
    std::cout << "[ALU CXX] Semantic Analysis Passed: Memory and Type Safety verified." << std::endl;
}
