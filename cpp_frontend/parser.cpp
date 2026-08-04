#include "parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> toks) : tokens(toks), pos(0) {}

Token Parser::currentToken() {
    if (pos >= tokens.size()) return {TokenType::TOK_EOF, ""};
    return tokens[pos];
}

void Parser::advance() {
    if (pos < tokens.size()) {
        std::cout << tokens[pos].value << " ";
        pos++;
    }
}

void Parser::expect(TokenType type) {
    if (currentToken().type != type) {
        throw std::runtime_error("Unexpected token: " + currentToken().value + " (expected " + std::to_string(static_cast<int>(type)) + ")");
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
    
    if (currentToken().type == TokenType::TOK_BIT_NOT) {
        advance();
        auto target = parseExpression();
        // Bitwise NOT is just XOR with -1 (all bits 1)
        auto negOne = std::make_unique<LiteralNode>(DataType::INT, "-1");
        left = std::make_unique<BinOpNode>("^", std::move(target), std::move(negOne));
    } else if (currentToken().type == TokenType::TOK_AMPERSAND) {
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
    } else if (currentToken().type == TokenType::TOK_INT_TYPE || 
               currentToken().type == TokenType::TOK_FLOAT_TYPE || 
               currentToken().type == TokenType::TOK_DOUBLE_TYPE || 
               currentToken().type == TokenType::TOK_BYTE_TYPE) {
        DataType targetType = DataType::INT;
        if (currentToken().type == TokenType::TOK_FLOAT_TYPE) targetType = DataType::FLOAT;
        else if (currentToken().type == TokenType::TOK_DOUBLE_TYPE) targetType = DataType::DOUBLE;
        else if (currentToken().type == TokenType::TOK_BYTE_TYPE) targetType = DataType::BYTE;
        advance(); // consume type token
        expect(TokenType::TOK_LPAREN);
        auto expr = parseExpression();
        expect(TokenType::TOK_RPAREN);
        left = std::make_unique<CastNode>(targetType, std::move(expr));
    } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
        std::string name = currentToken().value;
        advance();
        if (currentToken().type == TokenType::TOK_LPAREN) {
            // Function call
            left = parseFuncCall(name);
        } else if (currentToken().type == TokenType::TOK_DOT) {
            // Member Access or Method Call
            advance(); // consume '.'
            std::string field = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            
            if (currentToken().type == TokenType::TOK_LPAREN) {
                // Method call: p.print()
                advance(); // consume '('
                std::vector<std::unique_ptr<ASTNode>> args;
                if (currentToken().type != TokenType::TOK_RPAREN) {
                    args.push_back(parseExpression());
                    while (currentToken().type == TokenType::TOK_COMMA) {
                        advance();
                        args.push_back(parseExpression());
                    }
                }
                expect(TokenType::TOK_RPAREN);
                auto obj = std::make_unique<VarAccessNode>(name);
                left = std::make_unique<MethodCallNode>(std::move(obj), field, std::move(args));
            } else {
                left = std::make_unique<MemberAccessNode>(name, field);
            }
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            // Array Indexing
            left = std::make_unique<VarAccessNode>(name);
            while (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto idx = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                left = std::make_unique<ArrayIndexNode>(std::move(left), std::move(idx));
            }
        } else {
            // Basic variable read
            left = std::make_unique<VarAccessNode>(name);
        }
    } else if (currentToken().type == TokenType::TOK_INT_LITERAL) {
        left = std::make_unique<LiteralNode>(DataType::INT, currentToken().value);
        advance();
    } else if (currentToken().type == TokenType::TOK_FLOAT_LITERAL) {
        left = std::make_unique<LiteralNode>(DataType::FLOAT, currentToken().value);
        advance();
    } else if (currentToken().type == TokenType::TOK_STRING) {
        left = std::make_unique<LiteralNode>(DataType::STRING, currentToken().value);
        advance();
    } else if (currentToken().type == TokenType::TOK_LPAREN) {
        advance();
        left = parseExpression();
        expect(TokenType::TOK_RPAREN);
    } else if (currentToken().type == TokenType::TOK_YIELD) {
        left = parseYieldStatement();
    } else {
        throw std::runtime_error("Expected expression, got: " + currentToken().value);
    }

    // Binary operations (including comparisons, arithmetic, and bitwise)
    if (currentToken().type == TokenType::TOK_PLUS || 
        currentToken().type == TokenType::TOK_MINUS ||
        currentToken().type == TokenType::TOK_STAR ||
        currentToken().type == TokenType::TOK_SLASH ||
        currentToken().type == TokenType::TOK_PERCENT ||
        currentToken().type == TokenType::TOK_DOUBLE_EQUALS ||
        currentToken().type == TokenType::TOK_NOT_EQUALS ||
        currentToken().type == TokenType::TOK_LESS_THAN ||
        currentToken().type == TokenType::TOK_LESS_EQUALS ||
        currentToken().type == TokenType::TOK_GREATER_THAN ||
        currentToken().type == TokenType::TOK_GREATER_EQUALS ||
        currentToken().type == TokenType::TOK_AMPERSAND ||
        currentToken().type == TokenType::TOK_BIT_OR ||
        currentToken().type == TokenType::TOK_BIT_XOR ||
        currentToken().type == TokenType::TOK_LSHIFT ||
        currentToken().type == TokenType::TOK_RSHIFT) {
        std::string op = currentToken().value;
        advance();
        std::unique_ptr<ASTNode> right = parseExpression();
        return std::make_unique<BinOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::string Parser::parseTypeString() {
    std::string typeStr = currentToken().value;
    advance();
    
    if (currentToken().type == TokenType::TOK_LESS_THAN) {
        typeStr += "<";
        advance();
        while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
            typeStr += parseTypeString();
            if (currentToken().type == TokenType::TOK_COMMA) {
                typeStr += ",";
                advance();
            }
        }
        typeStr += ">";
        expect(TokenType::TOK_GREATER_THAN);
    }
    
    while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
        if (currentToken().type == TokenType::TOK_STAR) {
            typeStr += "*";
            advance();
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            advance();
            expect(TokenType::TOK_RBRACKET);
            typeStr += "[]";
        }
    }
    return typeStr;
}

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    std::string typeStr = parseTypeString();

    
    // pointers/arrays are handled in parseTypeString
    
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

std::unique_ptr<ForNode> Parser::parseForStatement() {
    expect(TokenType::TOK_FOR);
    expect(TokenType::TOK_LPAREN);
    
    // Init: can be a var decl (int i = 0) or assignment (i = 0) or empty
    std::unique_ptr<ASTNode> init = nullptr;
    if (currentToken().type != TokenType::TOK_SEMICOLON) {
        if (currentToken().type == TokenType::TOK_INT_TYPE || currentToken().type == TokenType::TOK_STRING_TYPE ||
            currentToken().type == TokenType::TOK_BOOL_TYPE || currentToken().type == TokenType::TOK_FLOAT_TYPE || 
            currentToken().type == TokenType::TOK_DOUBLE_TYPE || currentToken().type == TokenType::TOK_BYTE_TYPE) {
            // Variable declaration without trailing semicolon (we consume it below)
            std::string typeStr = parseTypeString();
            std::string name = currentToken().value;
            advance();
            std::unique_ptr<ASTNode> expr = nullptr;
            if (currentToken().type == TokenType::TOK_EQUALS) {
                advance();
                expr = parseExpression();
            }
            init = std::make_unique<VarDeclNode>(typeStr, name, std::move(expr));
        } else {
            // Assignment: identifier = expr
            std::string name = currentToken().value;
            advance();
            expect(TokenType::TOK_EQUALS);
            auto expr = parseExpression();
            init = std::make_unique<VarAssignNode>(name, std::move(expr));
        }
    }
    expect(TokenType::TOK_SEMICOLON);
    
    // Condition
    std::unique_ptr<ASTNode> cond = nullptr;
    if (currentToken().type != TokenType::TOK_SEMICOLON) {
        cond = parseExpression();
    }
    expect(TokenType::TOK_SEMICOLON);
    
    // Update: assignment or expression
    std::unique_ptr<ASTNode> update = nullptr;
    if (currentToken().type != TokenType::TOK_RPAREN) {
        std::string name = currentToken().value;
        advance();
        expect(TokenType::TOK_EQUALS);
        auto expr = parseExpression();
        update = std::make_unique<VarAssignNode>(name, std::move(expr));
    }
    expect(TokenType::TOK_RPAREN);
    
    auto forNode = std::make_unique<ForNode>(std::move(init), std::move(cond), std::move(update));
    forNode->body = parseBlock();
    return forNode;
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

std::unique_ptr<TryCatchNode> Parser::parseTryCatchStatement() {
    expect(TokenType::TOK_TRY);
    auto try_body = parseBlock();
    
    expect(TokenType::TOK_CATCH);
    expect(TokenType::TOK_LPAREN);
    std::string catch_type = parseTypeString();
    std::string catch_name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_RPAREN);
    
    auto catch_body = parseBlock();
    
    return std::make_unique<TryCatchNode>(std::move(try_body), catch_type, catch_name, std::move(catch_body));
}

std::unique_ptr<ThrowNode> Parser::parseThrowStatement() {
    expect(TokenType::TOK_THROW);
    auto expr = parseExpression();
    expect(TokenType::TOK_SEMICOLON);
    return std::make_unique<ThrowNode>(std::move(expr));
}

std::unique_ptr<EffectDeclNode> Parser::parseEffectDecl() {
    expect(TokenType::TOK_EFFECT);
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    
    auto effect = std::make_unique<EffectDeclNode>(name);
    while (currentToken().type == TokenType::TOK_ROUTINE) {
        // Parse it like a normal routine but without the body, actually our parseRoutine expects a body.
        // We'll just read the signature here to save time since it's an interface.
        expect(TokenType::TOK_ROUTINE);
        std::string mName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        expect(TokenType::TOK_LPAREN);
        
        std::vector<std::pair<std::string, std::string>> params;
        while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
            std::string pType = parseTypeString();
            std::string pName = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            params.push_back({pType, pName});
            if (currentToken().type == TokenType::TOK_COMMA) advance();
        }
        expect(TokenType::TOK_RPAREN);
        expect(TokenType::TOK_ARROW);
        std::string retType = parseTypeString();
        expect(TokenType::TOK_SEMICOLON);
        
        auto routine = std::make_unique<RoutineNode>(mName, std::nullopt, false);
        for (const auto& p : params) routine->params.push_back({p.first, p.second});
        routine->returnType = retType;
        // No body
        effect->methods.push_back(std::move(routine));
    }
    expect(TokenType::TOK_RBRACE);
    return effect;
}

std::unique_ptr<HandleNode> Parser::parseHandleStatement() {
    expect(TokenType::TOK_HANDLE);
    std::string effect_name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    
    auto handle = std::make_unique<HandleNode>(effect_name);
    
    expect(TokenType::TOK_ON);
    std::string mName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    handle->handler_method = mName;
    
    expect(TokenType::TOK_LPAREN);
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        std::string pType = parseTypeString();
        std::string pName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        handle->handler_args.push_back({DataType::UNKNOWN, pName}); // simple parse
        if (currentToken().type == TokenType::TOK_COMMA) advance();
    }
    expect(TokenType::TOK_RPAREN);
    
    handle->handler_body = parseBlock();
    
    expect(TokenType::TOK_RBRACE);
    expect(TokenType::TOK_IN);
    
    std::string inFuncName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    handle->in_call = parseFuncCall(inFuncName);
    expect(TokenType::TOK_SEMICOLON);
    
    return handle;
}

std::unique_ptr<YieldNode> Parser::parseYieldStatement() {
    expect(TokenType::TOK_YIELD);
    std::string eName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_DOT);
    std::string mName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LPAREN);
    
    auto yieldNode = std::make_unique<YieldNode>(eName, mName);
    
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        yieldNode->args.push_back(parseExpression());
        if (currentToken().type == TokenType::TOK_COMMA) advance();
    }
    expect(TokenType::TOK_RPAREN);
    
    return yieldNode;
}

std::unique_ptr<ResumeNode> Parser::parseResumeStatement() {
    expect(TokenType::TOK_RESUME);
    expect(TokenType::TOK_LPAREN);
    auto expr = parseExpression();
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_SEMICOLON);
    return std::make_unique<ResumeNode>(std::move(expr));
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
    } else if (currentToken().type == TokenType::TOK_INT_TYPE || currentToken().type == TokenType::TOK_STRING_TYPE || currentToken().type == TokenType::TOK_BOOL_TYPE || currentToken().type == TokenType::TOK_FLOAT_TYPE || currentToken().type == TokenType::TOK_DOUBLE_TYPE || currentToken().type == TokenType::TOK_BYTE_TYPE) {
        return parseVarDecl();
    } else if (currentToken().type == TokenType::TOK_IF) {
        return parseIfStatement();
    } else if (currentToken().type == TokenType::TOK_WHILE) {
        return parseWhileStatement();
    } else if (currentToken().type == TokenType::TOK_FOR) {
        return parseForStatement();
    } else if (currentToken().type == TokenType::TOK_TRY) {
        return parseTryCatchStatement();
    } else if (currentToken().type == TokenType::TOK_THROW) {
        return parseThrowStatement();
    } else if (currentToken().type == TokenType::TOK_RETURN) {
        return parseReturnStatement();
    } else if (currentToken().type == TokenType::TOK_HANDLE) {
        return parseHandleStatement();
    } else if (currentToken().type == TokenType::TOK_RESUME) {
        return parseResumeStatement();
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
        
        if (currentToken().type == TokenType::TOK_LESS_THAN) {
            std::string typeStr = name;
            typeStr += "<";
            advance();
            while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
                typeStr += parseTypeString();
                if (currentToken().type == TokenType::TOK_COMMA) {
                    typeStr += ",";
                    advance();
                }
            }
            typeStr += ">";
            expect(TokenType::TOK_GREATER_THAN);
            
            while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
                if (currentToken().type == TokenType::TOK_STAR) {
                    typeStr += "*";
                    advance();
                } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                    advance();
                    expect(TokenType::TOK_RBRACKET);
                    typeStr += "[]";
                }
            }
            
            std::string varName = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            
            if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto sizeExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                expect(TokenType::TOK_SEMICOLON);
                return std::make_unique<ArrayDeclNode>(typeStr, varName, std::move(sizeExpr));
            }
            
            std::unique_ptr<ASTNode> expr = nullptr;
            if (currentToken().type == TokenType::TOK_EQUALS) {
                advance();
                expr = parseExpression();
            }
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<VarDeclNode>(typeStr, varName, std::move(expr));
        } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
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
            std::unique_ptr<ASTNode> arrayExpr = std::make_unique<VarAccessNode>(name);
            while (true) {
                advance(); // Consume '['
                auto idxExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                
                if (currentToken().type == TokenType::TOK_LBRACKET) {
                    arrayExpr = std::make_unique<ArrayIndexNode>(std::move(arrayExpr), std::move(idxExpr));
                } else {
                    expect(TokenType::TOK_EQUALS);
                    auto valExpr = parseExpression();
                    expect(TokenType::TOK_SEMICOLON);
                    return std::make_unique<ArrayAssignNode>(std::move(arrayExpr), std::move(idxExpr), std::move(valExpr));
                }
            }
        } else if (currentToken().type == TokenType::TOK_EQUALS) {
            advance(); // Consume '='
            auto expr = parseExpression();
            expect(TokenType::TOK_SEMICOLON);
            return std::make_unique<VarAssignNode>(name, std::move(expr));
        } else if (currentToken().type == TokenType::TOK_DOT) {
            advance(); // consume '.'
            std::string field = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            
            if (currentToken().type == TokenType::TOK_LPAREN) {
                advance(); // consume '('
                std::vector<std::unique_ptr<ASTNode>> args;
                if (currentToken().type != TokenType::TOK_RPAREN) {
                    args.push_back(parseExpression());
                    while (currentToken().type == TokenType::TOK_COMMA) {
                        advance();
                        args.push_back(parseExpression());
                    }
                }
                expect(TokenType::TOK_RPAREN);
                expect(TokenType::TOK_SEMICOLON);
                auto obj = std::make_unique<VarAccessNode>(name);
                return std::make_unique<MethodCallNode>(std::move(obj), field, std::move(args));
            } else {
                expect(TokenType::TOK_EQUALS);
                auto expr = parseExpression();
                expect(TokenType::TOK_SEMICOLON);
                return std::make_unique<MemberAssignNode>(name, field, std::move(expr));
            }
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
    bool isExported = false;
    if (currentToken().type == TokenType::TOK_EXPORT) {
        isExported = true;
        advance();
    }
    expect(TokenType::TOK_ROUTINE);
    
    std::optional<Receiver> receiver = std::nullopt;
    
    // Check for receiver
    if (currentToken().type == TokenType::TOK_LPAREN) {
        advance();
        std::string rName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        expect(TokenType::TOK_COLON);
        std::string rType = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        bool isPtr = false;
        if (currentToken().type == TokenType::TOK_STAR) {
            isPtr = true;
            advance();
        }
        expect(TokenType::TOK_RPAREN);
        receiver = Receiver{rName, rType, isPtr};
    }
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error("Expected routine name");
    }
    std::string name = currentToken().value;
    advance();
    
    if (receiver) {
        name = receiver->type + "_" + name;
    }
    
    auto routine = std::make_unique<RoutineNode>(name, receiver, isExported);
    
    if (receiver) {
        std::string pType = receiver->type;
        if (receiver->isPointer) pType += "*";
        routine->params.push_back({pType, receiver->name});
    }
    
    expect(TokenType::TOK_LPAREN);
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        std::string pType = currentToken().value;
        advance();
        while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
            if (currentToken().type == TokenType::TOK_STAR) {
                pType += "*";
                advance();
            } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                expect(TokenType::TOK_RBRACKET);
                pType += "[]";
            }
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
        while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
            if (currentToken().type == TokenType::TOK_STAR) {
                routine->returnType += "*";
                advance();
            } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                expect(TokenType::TOK_RBRACKET);
                routine->returnType += "[]";
            }
        }
    }
    
    routine->body = parseBlock();
    
    return routine;
}

std::unique_ptr<ExternRoutineNode> Parser::parseExternRoutine() {
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
        while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
            if (currentToken().type == TokenType::TOK_STAR) {
                type += "*";
                advance();
            } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                expect(TokenType::TOK_RBRACKET);
                type += "[]";
            }
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
        while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
            if (currentToken().type == TokenType::TOK_STAR) {
                returnType += "*";
                advance();
            } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                expect(TokenType::TOK_RBRACKET);
                returnType += "[]";
            }
        }
    }
    expect(TokenType::TOK_SEMICOLON);
    
    return std::make_unique<ExternRoutineNode>(name, params, isVariadic, returnType);
}

std::unique_ptr<StructDefNode> Parser::parseStructDef() {
    expect(TokenType::TOK_STRUCT);
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    
    std::vector<std::string> type_params;
    if (currentToken().type == TokenType::TOK_LESS_THAN) {
        advance();
        while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
            type_params.push_back(currentToken().value);
            expect(TokenType::TOK_IDENTIFIER);
            if (currentToken().type == TokenType::TOK_COMMA) {
                advance();
            }
        }
        expect(TokenType::TOK_GREATER_THAN);
    }
    
    expect(TokenType::TOK_LBRACE);
    std::vector<StructField> fields;
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        std::string typeStr = parseTypeString();
        std::string fieldName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        expect(TokenType::TOK_SEMICOLON);
        fields.push_back({typeStr, fieldName});
    }
    expect(TokenType::TOK_RBRACE);
    return std::make_unique<StructDefNode>(name, type_params, fields);
}

std::unique_ptr<ImportNode> Parser::parseImport() {
    expect(TokenType::TOK_IMPORT);
    std::string moduleName = currentToken().value;
    expect(TokenType::TOK_STRING);
    expect(TokenType::TOK_SEMICOLON);
    return std::make_unique<ImportNode>(moduleName);
}



std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    
    while (currentToken().type != TokenType::TOK_EOF) {
        if (currentToken().type == TokenType::TOK_ROUTINE || currentToken().type == TokenType::TOK_EXPORT) {
            program->declarations.push_back(parseRoutine());
        } else if (currentToken().type == TokenType::TOK_EXTERN) {
            program->declarations.push_back(parseExternRoutine());
        } else if (currentToken().type == TokenType::TOK_STRUCT) {
            program->declarations.push_back(parseStructDef());
        } else if (currentToken().type == TokenType::TOK_IMPORT) {
            program->declarations.push_back(parseImport());
        } else if (currentToken().type == TokenType::TOK_EFFECT) {
            program->declarations.push_back(parseEffectDecl());
        } else {
            throw std::runtime_error("Unexpected token at top level: " + currentToken().value);
        }
    }
    
    return program;
}
