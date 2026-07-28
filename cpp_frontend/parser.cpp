#include "parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> toks) : tokens(toks), pos(0) {}

Token Parser::currentToken() {
    if (pos >= tokens.size()) return {TokenType::TOK_EOF, ""};
    return tokens[pos];
}

void Parser::advance() {
    if (pos < tokens.size()) pos++;
}

void Parser::expect(TokenType type) {
    if (currentToken().type != type) {
        throw std::runtime_error("Unexpected token: " + currentToken().value);
    }
    advance();
}

std::unique_ptr<AsmCallNode> Parser::parseAsmCall() {
    expect(TokenType::TOK_ASM);
    expect(TokenType::TOK_LPAREN);
    
    if (currentToken().type != TokenType::TOK_STRING) {
        throw std::runtime_error("Expected string literal in asm call");
    }
    std::string instr = currentToken().value;
    advance();
    
    expect(TokenType::TOK_RPAREN);
    return std::make_unique<AsmCallNode>(instr);
}

std::unique_ptr<UnsafeBlockNode> Parser::parseUnsafeBlock() {
    expect(TokenType::TOK_UNSAFE);
    expect(TokenType::TOK_LBRACE);
    
    auto block = std::make_unique<UnsafeBlockNode>();
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        block->body.push_back(parseStatement());
    }
    
    expect(TokenType::TOK_RBRACE);
    return block;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    // Very simple expression parser for literal + literal
    std::unique_ptr<ASTNode> left;
    if (currentToken().type == TokenType::TOK_INT_LITERAL) {
        left = std::make_unique<LiteralNode>(DataType::INT, currentToken().value);
        advance();
    } else if (currentToken().type == TokenType::TOK_STRING) {
        left = std::make_unique<LiteralNode>(DataType::STRING, currentToken().value);
        advance();
    } else {
        throw std::runtime_error("Expected expression");
    }

    if (currentToken().type == TokenType::TOK_PLUS) {
        std::string op = currentToken().value;
        advance();
        std::unique_ptr<ASTNode> right = parseExpression();
        return std::make_unique<BinOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<VarDeclNode> Parser::parseVarDecl() {
    std::string typeStr = currentToken().value; // "int" or "string"
    advance();
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error("Expected variable name");
    }
    std::string name = currentToken().value;
    advance();
    
    expect(TokenType::TOK_EQUALS);
    
    std::unique_ptr<ASTNode> expr = parseExpression();
    
    expect(TokenType::TOK_SEMICOLON);
    
    return std::make_unique<VarDeclNode>(typeStr, name, std::move(expr));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (currentToken().type == TokenType::TOK_UNSAFE) {
        return parseUnsafeBlock();
    } else if (currentToken().type == TokenType::TOK_ASM) {
        return parseAsmCall();
    } else if (currentToken().type == TokenType::TOK_INT_TYPE || currentToken().type == TokenType::TOK_STRING_TYPE) {
        return parseVarDecl();
    } else {
        // Skip unknown tokens for this simple prototype
        advance();
        return nullptr;
    }
}

std::unique_ptr<RoutineNode> Parser::parseRoutine() {
    expect(TokenType::TOK_ROUTINE);
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error("Expected routine name");
    }
    std::string name = currentToken().value;
    advance();
    
    expect(TokenType::TOK_LPAREN);
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_LBRACE);
    
    auto routine = std::make_unique<RoutineNode>(name);
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        auto stmt = parseStatement();
        if (stmt) {
            routine->body.push_back(std::move(stmt));
        }
    }
    
    expect(TokenType::TOK_RBRACE);
    return routine;
}

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    
    while (currentToken().type != TokenType::TOK_EOF) {
        if (currentToken().type == TokenType::TOK_ROUTINE) {
            program->declarations.push_back(parseRoutine());
        } else {
            advance(); // Skip comments/imports for this simple parse tree
        }
    }
    
    return program;
}
