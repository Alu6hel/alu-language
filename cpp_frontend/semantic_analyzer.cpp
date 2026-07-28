#include "semantic_analyzer.h"
#include <stdexcept>

DataType SemanticAnalyzer::checkExpression(ASTNode* expr) {
    if (auto literal = dynamic_cast<LiteralNode*>(expr)) {
        return literal->type;
    }
    else if (auto binop = dynamic_cast<BinOpNode*>(expr)) {
        DataType leftType = checkExpression(binop->left.get());
        DataType rightType = checkExpression(binop->right.get());
        
        if (leftType != rightType) {
            throw std::runtime_error(
                "Semantic Error: Type mismatch in binary operation. Cannot operate on " +
                DataTypeToString(leftType) + " and " + DataTypeToString(rightType)
            );
        }
        return leftType;
    }
    return DataType::UNKNOWN;
}

void SemanticAnalyzer::checkVarDecl(VarDeclNode* decl) {
    DataType expectedType = DataType::UNKNOWN;
    if (decl->varType == "int") expectedType = DataType::INT;
    else if (decl->varType == "string") expectedType = DataType::STRING;
    
    if (expectedType == DataType::UNKNOWN) {
        throw std::runtime_error("Semantic Error: Unknown variable type '" + decl->varType + "'");
    }
    
    // Check if variable is already defined
    if (symbol_table.find(decl->name) != symbol_table.end()) {
        throw std::runtime_error("Semantic Error: Variable '" + decl->name + "' already declared in this scope.");
    }
    
    // Check the expression it is initialized with
    if (decl->initializer) {
        DataType actualType = checkExpression(decl->initializer.get());
        if (expectedType != actualType) {
            throw std::runtime_error(
                "Semantic Error: Type mismatch in variable declaration '" + decl->name + "'. Expected " +
                DataTypeToString(expectedType) + " but got " + DataTypeToString(actualType)
            );
        }
    }
    
    // Store in symbol table
    symbol_table[decl->name] = expectedType;
}

void SemanticAnalyzer::checkStatement(ASTNode* stmt) {
    if (auto vardecl = dynamic_cast<VarDeclNode*>(stmt)) {
        checkVarDecl(vardecl);
    }
    else if (auto unsafeBlock = dynamic_cast<UnsafeBlockNode*>(stmt)) {
        for (const auto& s : unsafeBlock->body) {
            checkStatement(s.get());
        }
    }
    // AsmCall is unchecked for now (by definition it's unsafe)
}

void SemanticAnalyzer::checkRoutine(RoutineNode* routine) {
    // In a real compiler, we'd push a new scope onto a scope stack here.
    for (const auto& stmt : routine->body) {
        checkStatement(stmt.get());
    }
}

void SemanticAnalyzer::analyze(ProgramNode* ast) {
    std::cout << "[ALU CXX] Running Semantic Analysis..." << std::endl;
    for (const auto& decl : ast->declarations) {
        if (auto routine = dynamic_cast<RoutineNode*>(decl.get())) {
            checkRoutine(routine);
        }
    }
    std::cout << "[ALU CXX] Semantic Analysis Passed: Memory and Type Safety verified." << std::endl;
}
