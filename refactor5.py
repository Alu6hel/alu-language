import re

with open('cpp_frontend/semantic_analyzer.cpp', 'r') as f:
    content = f.read()

# 1. In checkVarDecl:
decl_replace = '''
    decl->varType = resolveName(decl->varType);
    DataType expectedType = parseDataType(decl->varType);
    std::string declared_unit = extractUnit(decl->varType);
'''
content = content.replace('decl->varType = resolveName(decl->varType);\n    DataType expectedType = parseDataType(decl->varType);', decl_replace.strip())

decl_assign_replace = '''
    if (decl->initializer) {
        TypeInfo actualType_info = checkExpression(decl->initializer.get());
        DataType actualType = actualType_info.type;
        std::string actual_unit = actualType_info.unit;
        if (!declared_unit.empty() && !actual_unit.empty() && declared_unit != actual_unit) {
            throw std::runtime_error("Semantic Error: Unit mismatch in variable declaration '" + decl->name + "'. Expected unit <" + declared_unit + "> but got <" + actual_unit + ">.");
        }
'''
content = content.replace('if (decl->initializer) {\n        TypeInfo actualType_info = checkExpression(decl->initializer.get());\n        DataType actualType = actualType_info.type;', decl_assign_replace.strip())

decl_sym_replace = '''
    // Store in current scope
    declareSymbol(decl->name, expectedType, declared_unit, decl->line, decl->col, decl->file);
'''
content = content.replace('// Store in current scope\n    declareSymbol(decl->name, expectedType, "", decl->line, decl->col, decl->file);', decl_sym_replace.strip())


# 2. In VarAccessNode:
var_acc_old = '''
    else if (auto varNode = dynamic_cast<VarAccessNode*>(expr)) {
        DataType t;
        if (!lookupSymbol(varNode->name, t)) {
'''
var_acc_new = '''
    else if (auto varNode = dynamic_cast<VarAccessNode*>(expr)) {
        SymbolMeta meta;
        if (!lookupSymbolMeta(varNode->name, meta)) {
'''
content = content.replace(var_acc_old.strip(), var_acc_new.strip())

var_acc_end_old = '''
        if (is_lsp_mode) {
            SymbolMeta meta;
            if (lookupSymbolMeta(varNode->name, meta)) {
                LSPSymbol sym;
                sym.line = varNode->line; sym.col = varNode->col; sym.length = varNode->name.length();
                sym.name = varNode->name; sym.hover_text = DataTypeToString(t);
                sym.def_file = meta.def_file; sym.def_line = meta.def_line; sym.def_col = meta.def_col;
                lsp_symbols.push_back(sym);
            }
        }
        return {t, ""};
'''
var_acc_end_new = '''
        if (is_lsp_mode) {
            LSPSymbol sym;
            sym.line = varNode->line; sym.col = varNode->col; sym.length = varNode->name.length();
            sym.name = varNode->name; sym.hover_text = DataTypeToString(meta.type) + (meta.unit.empty() ? "" : "<" + meta.unit + ">");
            sym.def_file = meta.def_file; sym.def_line = meta.def_line; sym.def_col = meta.def_col;
            lsp_symbols.push_back(sym);
        }
        return {meta.type, meta.unit};
'''
content = content.replace(var_acc_end_old.strip(), var_acc_end_new.strip())

# 3. In BinOpNode:
binop_old = '''
        if (expected == DataType::UNKNOWN) {
            if (leftT == DataType::STRING || rightT == DataType::STRING) {
                if (binOp->op != "+") {
                    if (!is_lsp_mode) { std::cerr << "BINOP STRING ERROR: " << std::endl; binOp->print(0); }
                    throw std::runtime_error("Semantic Error: Invalid operator '" + binOp->op + "' for string. Only '+' is supported.");
                }
                return {DataType::STRING, ""};
            }
            if (leftT != rightT && leftT != DataType::UNKNOWN && rightT != DataType::UNKNOWN) {
                if (!((leftT == DataType::INT && rightT == DataType::FLOAT) || (leftT == DataType::FLOAT && rightT == DataType::INT))) {
                    if (!is_lsp_mode) { std::cerr << "BINOP TYPE ERROR: " << std::endl; binOp->print(0); }
                    throw std::runtime_error("Semantic Error: Type mismatch in binary operation (" + DataTypeToString(leftT) + " " + binOp->op + " " + DataTypeToString(rightT) + ")");
                }
            }
            return {leftT != DataType::UNKNOWN ? leftT : rightT, ""};
        }
        return {expected, ""};
'''

binop_new = '''
        std::string res_unit = "";
        if (!leftT_info.unit.empty() || !rightT_info.unit.empty()) {
            if (binOp->op == "+" || binOp->op == "-") {
                if (leftT_info.unit != rightT_info.unit && (!leftT_info.unit.empty() && !rightT_info.unit.empty())) {
                    throw std::runtime_error("Semantic Error: Unit mismatch in addition/subtraction. <" + leftT_info.unit + "> vs <" + rightT_info.unit + ">");
                }
                res_unit = leftT_info.unit.empty() ? rightT_info.unit : leftT_info.unit;
            } else if (binOp->op == "*") {
                auto lu = parseUnit(leftT_info.unit);
                auto ru = parseUnit(rightT_info.unit);
                for (auto& kv : ru) lu[kv.first] += kv.second;
                res_unit = formatUnit(lu);
            } else if (binOp->op == "/") {
                auto lu = parseUnit(leftT_info.unit);
                auto ru = parseUnit(rightT_info.unit);
                for (auto& kv : ru) lu[kv.first] -= kv.second;
                res_unit = formatUnit(lu);
            } else {
                res_unit = leftT_info.unit.empty() ? rightT_info.unit : leftT_info.unit;
            }
        }
        if (expected == DataType::UNKNOWN) {
            if (leftT == DataType::STRING || rightT == DataType::STRING) {
                if (binOp->op != "+") {
                    if (!is_lsp_mode) { std::cerr << "BINOP STRING ERROR: " << std::endl; binOp->print(0); }
                    throw std::runtime_error("Semantic Error: Invalid operator '" + binOp->op + "' for string. Only '+' is supported.");
                }
                return {DataType::STRING, res_unit};
            }
            if (leftT != rightT && leftT != DataType::UNKNOWN && rightT != DataType::UNKNOWN) {
                if (!((leftT == DataType::INT && rightT == DataType::FLOAT) || (leftT == DataType::FLOAT && rightT == DataType::INT))) {
                    if (!is_lsp_mode) { std::cerr << "BINOP TYPE ERROR: " << std::endl; binOp->print(0); }
                    throw std::runtime_error("Semantic Error: Type mismatch in binary operation (" + DataTypeToString(leftT) + " " + binOp->op + " " + DataTypeToString(rightT) + ")");
                }
            }
            return {leftT != DataType::UNKNOWN ? leftT : rightT, res_unit};
        }
        return {expected, res_unit};
'''
content = content.replace(binop_old.strip(), binop_new.strip())

# 4. In VarAssignNode:
assign_old = '''
    else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        TypeInfo exprType_info = checkExpression(varassign->expr.get());
        DataType exprType = exprType_info.type;
        DataType expectedType;
'''
assign_new = '''
    else if (auto varassign = dynamic_cast<VarAssignNode*>(stmt)) {
        TypeInfo exprType_info = checkExpression(varassign->expr.get());
        DataType exprType = exprType_info.type;
        std::string actual_unit = exprType_info.unit;
        DataType expectedType;
'''
content = content.replace(assign_old.strip(), assign_new.strip())

assign_err_old = '''
            if (expectedType != DataType::UNKNOWN && expectedType != exprType && exprType != DataType::POINTER && exprType != DataType::UNKNOWN) {
'''
assign_err_new = '''
            SymbolMeta meta;
            if (lookupSymbolMeta(varassign->name, meta)) {
                if (!meta.unit.empty() && !actual_unit.empty() && meta.unit != actual_unit) {
                    throw std::runtime_error("Semantic Error: Unit mismatch in assignment to '" + varassign->name + "'. Expected unit <" + meta.unit + "> but got <" + actual_unit + ">.");
                }
            }
            if (expectedType != DataType::UNKNOWN && expectedType != exprType && exprType != DataType::POINTER && exprType != DataType::UNKNOWN) {
'''
content = content.replace(assign_err_old.strip(), assign_err_new.strip())


with open('cpp_frontend/semantic_analyzer.cpp', 'w') as f:
    f.write(content)

print('Refactor 5 finished.')
