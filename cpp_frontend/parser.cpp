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
    expect(TokenType::TOK_SEMICOLON);
    return std::make_unique<AsmCallNode>(instr);
}

std::unique_ptr<UnsafeBlockNode> Parser::parseUnsafeBlock() {
    expect(TokenType::TOK_UNSAFE);
    expect(TokenType::TOK_LBRACE);
    
    auto block = std::make_unique<UnsafeBlockNode>();
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        auto stmt = parseStatement();
        if (stmt) {
            block->body.push_back(std::move(stmt));
        }
    }
    
    expect(TokenType::TOK_RBRACE);
    return block;
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    // Left side
    std::unique_ptr<ASTNode> left;
    
    if (currentToken().type == TokenType::TOK_AMPERSAND) {
        advance();
        left = std::make_unique<AddressOfNode>(parseExpression());
    } else if (currentToken().type == TokenType::TOK_STAR) {
        advance();
        left = std::make_unique<DereferenceNode>(parseExpression());
    } else if (currentToken().type == TokenType::TOK_NEW) {
        advance();
        std::string typeName = currentToken().value;
        if (currentToken().type == TokenType::TOK_IDENTIFIER || 
            currentToken().type == TokenType::TOK_INT_TYPE || 
            currentToken().type == TokenType::TOK_STRING_TYPE) {
            advance();
        } else {
            throw std::runtime_error("Expected type name after 'new'");
        }
        left = std::make_unique<NewAllocationNode>(typeName);
    } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
        std::string name = currentToken().value;
        advance();
        if (currentToken().type == TokenType::TOK_LPAREN) {
            // Function call
            left = parseFuncCall(name);
        } else if (currentToken().type == TokenType::TOK_DOT) {
            // Member Access
            advance(); // consume '.'
            std::string field = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            left = std::make_unique<MemberAccessNode>(name, field);
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            // Array Indexing
            advance();
            auto idx = parseExpression();
            expect(TokenType::TOK_RBRACKET);
            left = std::make_unique<ArrayIndexNode>(name, std::move(idx));
        } else {
            // Basic variable read
            left = std::make_unique<VarAccessNode>(name);
        }
    } else if (currentToken().type == TokenType::TOK_INT_LITERAL) {
        left = std::make_unique<LiteralNode>(DataType::INT, currentToken().value);
        advance();
    } else if (currentToken().type == TokenType::TOK_STRING) {
        left = std::make_unique<LiteralNode>(DataType::STRING, currentToken().value);
        advance();
    } else {
        throw std::runtime_error("Expected expression");
    }

    // Binary operations (including comparisons)
    if (currentToken().type == TokenType::TOK_PLUS || 
        currentToken().type == TokenType::TOK_DOUBLE_EQUALS ||
        currentToken().type == TokenType::TOK_LESS_THAN ||
        currentToken().type == TokenType::TOK_GREATER_THAN) {
        std::string op = currentToken().value;
        advance();
        std::unique_ptr<ASTNode> right = parseExpression();
        return std::make_unique<BinOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    std::string typeStr = currentToken().value; // "int" or "string"
    advance();
    
    if (currentToken().type == TokenType::TOK_STAR) {
        typeStr += "*";
        advance();
    }
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error("Expected variable name");
    }
    std::string name = currentToken().value;
    advance();
    
    if (currentToken().type == TokenType::TOK_LBRACKET) {
        advance();
        auto sizeExpr = parseExpression();
        expect(TokenType::TOK_RBRACKET);
        expect(TokenType::TOK_SEMICOLON);
        return std::make_unique<ArrayDeclNode>(typeStr, name, std::move(sizeExpr));
    }
    
    std::unique_ptr<ASTNode> expr = nullptr;
    if (currentToken().type == TokenType::TOK_EQUALS) {
        advance();
        expr = parseExpression();
    }
    
    expect(TokenType::TOK_SEMICOLON);
    
    return std::make_unique<VarDeclNode>(typeStr, name, std::move(expr));
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseBlock() {
    std::vector<std::unique_ptr<ASTNode>> block;
    expect(TokenType::TOK_LBRACE);
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        auto stmt = parseStatement();
        if (stmt) block.push_back(std::move(stmt));
    }
    expect(TokenType::TOK_RBRACE);
    return block;
}

std::unique_ptr<IfNode> Parser::parseIfStatement() {
    expect(TokenType::TOK_IF);
    expect(TokenType::TOK_LPAREN);
    std::unique_ptr<ASTNode> cond = parseExpression();
    expect(TokenType::TOK_RPAREN);
    
    auto ifNode = std::make_unique<IfNode>(std::move(cond));
    ifNode->then_body = parseBlock();
    
    if (currentToken().type == TokenType::TOK_ELSE) {
        advance();
        ifNode->else_body = parseBlock();
    }
    return ifNode;
}

std::unique_ptr<WhileNode> Parser::parseWhileStatement() {
    expect(TokenType::TOK_WHILE);
    expect(TokenType::TOK_LPAREN);
    std::unique_ptr<ASTNode> cond = parseExpression();
    expect(TokenType::TOK_RPAREN);
    
    auto whileNode = std::make_unique<WhileNode>(std::move(cond));
    whileNode->body = parseBlock();
    return whileNode;
}

std::unique_ptr<ReturnNode> Parser::parseReturnStatement() {
    expect(TokenType::TOK_RETURN);
    std::unique_ptr<ASTNode> expr = nullptr;
    if (currentToken().type != TokenType::TOK_SEMICOLON) {
        expr = parseExpression();
    }
    expect(TokenType::TOK_SEMICOLON);
    return std::make_unique<ReturnNode>(std::move(expr));
}

std::unique_ptr<FuncCallNode> Parser::parseFuncCall(std::string name) {
    expect(TokenType::TOK_LPAREN);
    std::vector<std::unique_ptr<ASTNode>> args;
    
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        args.push_back(parseExpression());
        if (currentToken().type == TokenType::TOK_COMMA) {
            advance();
        }
    }
    expect(TokenType::TOK_RPAREN);
    
    return std::make_unique<FuncCallNode>(name, std::move(args));
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (currentToken().type == TokenType::TOK_UNSAFE) {
        return parseUnsafeBlock();
    } else if (currentToken().type == TokenType::TOK_ASM) {
        return parseAsmCall();
    } else if (currentToken().type == TokenType::TOK_FREE) {
        advance();
        auto expr = parseExpression();
        expect(TokenType::TOK_SEMICOLON);
        return std::make_unique<FreeNode>(std::move(expr));
    } else if (currentToken().type == TokenType::TOK_INT_TYPE || currentToken().type == TokenType::TOK_STRING_TYPE) {
        return parseVarDecl();
    } else if (currentToken().type == TokenType::TOK_IF) {
        return parseIfStatement();
    } else if (currentToken().type == TokenType::TOK_WHILE) {
        return parseWhileStatement();
    } else if (currentToken().type == TokenType::TOK_RETURN) {
        return parseReturnStatement();
    } else if (currentToken().type == TokenType::TOK_STAR) {
        advance();
        auto target = parseExpression();
        expect(TokenType::TOK_EQUALS);
        auto val = parseExpression();
        expect(TokenType::TOK_SEMICOLON);
        return std::make_unique<DerefAssignNode>(std::move(target), std::move(val));
    } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
        // Could be a function call statement, assignment, member assignment, or custom type variable declaration
        std::string name = currentToken().value;
        advance();
        
        if (currentToken().type == TokenType::TOK_IDENTIFIER) {
            // It's a variable declaration: CustomType varName;
            std::string varName = currentToken().value;
            advance();
            
            if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto sizeExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                expect(TokenType::TOK_SEMICOLON);
                return std::make_unique<ArrayDeclNode>(name, varName, std::move(sizeExpr));
            }
            
            std::unique_ptr<ASTNode> expr = nullptr;
            if (currentToken().type == TokenType::TOK_EQUALS) {
                advance();
                expr = parseExpression();
            }
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<VarDeclNode>(name, varName, std::move(expr));
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            advance();
            auto idxExpr = parseExpression();
            expect(TokenType::TOK_RBRACKET);
            expect(TokenType::TOK_EQUALS);
            auto valExpr = parseExpression();
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<ArrayAssignNode>(name, std::move(idxExpr), std::move(valExpr));
        } else if (currentToken().type == TokenType::TOK_EQUALS) {
            advance(); // Consume '='
            auto expr = parseExpression();
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<VarAssignNode>(name, std::move(expr));
        } else if (currentToken().type == TokenType::TOK_DOT) {
            advance(); // consume '.'
            std::string field = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            expect(TokenType::TOK_EQUALS);
            auto expr = parseExpression();
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<MemberAssignNode>(name, field, std::move(expr));
        } else {
            auto call = parseFuncCall(name);
            expect(TokenType::TOK_SEMICOLON);
            return call;
        }
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
    
    auto routine = std::make_unique<RoutineNode>(name);
    
    expect(TokenType::TOK_LPAREN);
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        std::string pType = currentToken().value;
        advance();
        if (currentToken().type == TokenType::TOK_STAR) {
            pType += "*";
            advance();
        }
        std::string pName = currentToken().value;
        advance();
        routine->params.push_back({pType, pName});
        
        if (currentToken().type == TokenType::TOK_COMMA) advance();
    }
    expect(TokenType::TOK_RPAREN);
    
    if (currentToken().type == TokenType::TOK_ARROW) {
        advance();
        routine->returnType = currentToken().value; // e.g. "int"
        advance();
        if (currentToken().type == TokenType::TOK_STAR) {
            routine->returnType += "*";
            advance();
        }
    }
    
    routine->body = parseBlock();
    
    return routine;
}

std::unique_ptr<ASTNode> Parser::parseExternRoutine() {
    expect(TokenType::TOK_EXTERN);
    expect(TokenType::TOK_ROUTINE);
    
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    
    expect(TokenType::TOK_LPAREN);
    std::vector<Parameter> params;
    bool isVariadic = false;
    
    while (currentToken().type != TokenType::TOK_RPAREN) {
        if (currentToken().type == TokenType::TOK_ELLIPSIS) {
            isVariadic = true;
            advance();
            break; // ... must be the last argument
        }
        std::string type = currentToken().value;
        advance(); // skip type
        if (currentToken().type == TokenType::TOK_STAR) {
            type += "*";
            advance();
        }
        std::string pname = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        params.push_back({type, pname});
        
        if (currentToken().type == TokenType::TOK_COMMA) {
            advance();
        } else {
            break;
        }
    }
    expect(TokenType::TOK_RPAREN);
    
    std::string returnType = "void"; // default
    if (currentToken().type == TokenType::TOK_ARROW) {
        advance();
        returnType = currentToken().value; // int, string, void
        advance();
        if (currentToken().type == TokenType::TOK_STAR) {
            returnType += "*";
            advance();
        }
    }
    expect(TokenType::TOK_SEMICOLON);
    
    return std::make_unique<ExternRoutineNode>(name, params, isVariadic, returnType);
}

std::unique_ptr<StructDefNode> Parser::parseStructDef() {
    expect(TokenType::TOK_STRUCT);
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    std::vector<StructField> fields;
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        std::string typeStr = currentToken().value;
        advance();
        if (currentToken().type == TokenType::TOK_STAR) {
            typeStr += "*";
            advance();
        }
        std::string fieldName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        expect(TokenType::TOK_SEMICOLON);
        fields.push_back({typeStr, fieldName});
    }
    expect(TokenType::TOK_RBRACE);
    return std::make_unique<StructDefNode>(name, fields);
}

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    
    while (currentToken().type != TokenType::TOK_EOF) {
        if (currentToken().type == TokenType::TOK_ROUTINE) {
            program->declarations.push_back(parseRoutine());
        } else if (currentToken().type == TokenType::TOK_EXTERN) {
            program->declarations.push_back(parseExternRoutine());
        } else if (currentToken().type == TokenType::TOK_STRUCT) {
            program->declarations.push_back(parseStructDef());
        } else {
            advance(); // Skip comments/imports for this simple parse tree
        }
    }
    
    return program;
}
