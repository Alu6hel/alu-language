#include "parser.h"
#include "error_reporter.h"
#include <stdexcept>
#include <iostream>

Parser::Parser(std::vector<Token> toks, std::string fname) : tokens(toks), pos(0), filename(fname) {}

Token Parser::currentToken() {
    if (pos >= tokens.size()) return {TokenType::TOK_EOF, "", 0, 0};
    return tokens[pos];
}

void Parser::advance() {
    if (pos < tokens.size()) {
        pos++;
    }
}

void Parser::expect(TokenType type) {
    if (currentToken().type != type) {
        throw std::runtime_error(ErrorReporter::formatError(
            "Unexpected token: " + currentToken().value + " (expected " + TokenTypeToString(type) + ")",
            filename,
            currentToken().line,
            currentToken().col
        ));
    }
    advance();
}

std::string Parser::parseQualifiedName() {
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    while (currentToken().type == TokenType::TOK_DOUBLE_COLON) {
        advance(); // consume ::
        name += "::" + currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
    }
    return name;
}

std::unique_ptr<AsmCallNode> Parser::parseAsmCall() {
    Token startTok = currentToken();
    expect(TokenType::TOK_ASM);
    expect(TokenType::TOK_LPAREN);
    
    if (currentToken().type != TokenType::TOK_STRING) {
        throw std::runtime_error(ErrorReporter::formatError("Expected string literal in asm call", filename, currentToken().line, currentToken().col));
    }
    std::string instr = currentToken().value;
    advance();
    
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<AsmCallNode>(instr), startTok);
}

std::unique_ptr<UnsafeBlockNode> Parser::parseUnsafeBlock() {
    Token startTok = currentToken();
    expect(TokenType::TOK_UNSAFE);
    expect(TokenType::TOK_LBRACE);
    
    auto block = attachLoc(std::make_unique<UnsafeBlockNode>(), startTok);
    
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
    Token startTok = currentToken();
    // Left side
    std::unique_ptr<ASTNode> left;
    
    if (currentToken().type == TokenType::TOK_BIT_NOT) {
        advance();
        auto target = parseExpression();
        // Bitwise NOT is just XOR with -1 (all bits 1)
        auto negOne = attachLoc(std::make_unique<LiteralNode>(DataType::INT, "-1"), startTok);
        left = attachLoc(std::make_unique<BinOpNode>("^", std::move(target), std::move(negOne)), startTok);
    } else if (currentToken().type == TokenType::TOK_AMPERSAND) {
        advance();
        left = attachLoc(std::make_unique<AddressOfNode>(parseExpression()), startTok);
    } else if (currentToken().type == TokenType::TOK_STAR) {
        advance();
        left = attachLoc(std::make_unique<DereferenceNode>(parseExpression()), startTok);
    } else if (currentToken().type == TokenType::TOK_NEW) {
        advance();
        std::string typeName = parseTypeString();
        left = attachLoc(std::make_unique<NewAllocationNode>(typeName), startTok);
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
        left = attachLoc(std::make_unique<CastNode>(targetType, std::move(expr)), startTok);
    } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
          std::string name = parseQualifiedName();
          if (currentToken().type == TokenType::TOK_LPAREN) {
              left = parseFuncCall(name);
          } else if (currentToken().type == TokenType::TOK_LESS_THAN) {
              bool isGeneric = false;
              int temp = pos;
              int nesting = 0;
              while (temp < tokens.size() && tokens[temp].type != TokenType::TOK_EOF && tokens[temp].type != TokenType::TOK_SEMICOLON) {
                  if (tokens[temp].type == TokenType::TOK_LESS_THAN) nesting++;
                  else if (tokens[temp].type == TokenType::TOK_GREATER_THAN) {
                      nesting--;
                      if (nesting == 0) {
                          if (temp + 1 < tokens.size() && tokens[temp + 1].type == TokenType::TOK_LPAREN) {
                              isGeneric = true;
                          }
                          break;
                      }
                  }
                  temp++;
              }
              
              if (isGeneric) {
                  std::vector<std::string> parsed_types;
                  advance();
                  while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
                      parsed_types.push_back(parseTypeString());
                      if (currentToken().type == TokenType::TOK_COMMA) {
                          advance();
                      }
                  }
                  expect(TokenType::TOK_GREATER_THAN);
                  if (currentToken().type == TokenType::TOK_LPAREN) {
                      auto call = parseFuncCall(name);
                      call->type_args = parsed_types;
                      left = std::move(call);
                  } else {
                     throw std::runtime_error(ErrorReporter::formatError("Unexpected token after generic types in expression", filename, currentToken().line, currentToken().col));
                  }
              } else {
                  left = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
              }
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
                auto obj = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
                left = attachLoc(std::make_unique<MethodCallNode>(std::move(obj), field, std::move(args)), startTok);
            } else {
                left = attachLoc(std::make_unique<MemberAccessNode>(name, field), startTok);
            }
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            // Array Indexing
            left = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
            while (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto idx = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                left = attachLoc(std::make_unique<ArrayIndexNode>(std::move(left), std::move(idx)), startTok);
            }
        } else {
            // Basic variable read
            left = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
        }
    } else if (currentToken().type == TokenType::TOK_INT_LITERAL) {
        left = attachLoc(std::make_unique<LiteralNode>(DataType::INT, currentToken().value), startTok);
        advance();
    } else if (currentToken().type == TokenType::TOK_FLOAT_LITERAL) {
        left = attachLoc(std::make_unique<LiteralNode>(DataType::FLOAT, currentToken().value), startTok);
        advance();
    } else if (currentToken().type == TokenType::TOK_STRING) {
        left = attachLoc(std::make_unique<LiteralNode>(DataType::STRING, currentToken().value), startTok);
        advance();
    } else if (currentToken().type == TokenType::TOK_LPAREN) {
        advance();
        left = parseExpression();
        expect(TokenType::TOK_RPAREN);
    } else if (currentToken().type == TokenType::TOK_YIELD) {
        left = parseYieldStatement();
    } else if (currentToken().type == TokenType::TOK_RETURN) {
        left = attachLoc(std::make_unique<VarAccessNode>("return"), startTok);
        advance();
    } else {
        throw std::runtime_error(ErrorReporter::formatError("Expected expression, got: " + currentToken().value, filename, currentToken().line, currentToken().col));
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
        return attachLoc(std::make_unique<BinOpNode>(op, std::move(left), std::move(right)), startTok);
    }
    return left;
}

std::string Parser::parseTypeString() {
    std::string typeStr;
    if (currentToken().type == TokenType::TOK_IDENTIFIER) {
        typeStr = parseQualifiedName();
    } else {
        typeStr = currentToken().value;
        advance();
    }
    
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
            if (pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::TOK_RBRACKET) {
                advance();
                expect(TokenType::TOK_RBRACKET);
                typeStr += "[]";
            } else {
                break;
            }
        }
    }
    return typeStr;
}

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    Token startTok = currentToken();
    std::string typeStr = parseTypeString();

    
    // pointers/arrays are handled in parseTypeString
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error(ErrorReporter::formatError("Expected variable name", filename, currentToken().line, currentToken().col));
    }
    std::string name = currentToken().value;
    advance();
    
    if (currentToken().type == TokenType::TOK_LBRACKET) {
        advance();
        auto sizeExpr = parseExpression();
        expect(TokenType::TOK_RBRACKET);
        expect(TokenType::TOK_SEMICOLON);
        return attachLoc(std::make_unique<ArrayDeclNode>(typeStr, name, std::move(sizeExpr)), startTok);
    }
    
    std::unique_ptr<ASTNode> expr = nullptr;
    if (currentToken().type == TokenType::TOK_EQUALS) {
        advance();
        expr = parseExpression();
    }
    
    expect(TokenType::TOK_SEMICOLON);
    
    return attachLoc(std::make_unique<VarDeclNode>(typeStr, name, std::move(expr)), startTok);
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
    Token startTok = currentToken();
    expect(TokenType::TOK_IF);
    expect(TokenType::TOK_LPAREN);
    std::unique_ptr<ASTNode> cond = parseExpression();
    expect(TokenType::TOK_RPAREN);
    
    auto ifNode = attachLoc(std::make_unique<IfNode>(std::move(cond)), startTok);
    ifNode->then_body = parseBlock();
    
    if (currentToken().type == TokenType::TOK_ELSE) {
        advance();
        ifNode->else_body = parseBlock();
    }
    return ifNode;
}

std::unique_ptr<WhileNode> Parser::parseWhileStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_WHILE);
    expect(TokenType::TOK_LPAREN);
    std::unique_ptr<ASTNode> cond = parseExpression();
    expect(TokenType::TOK_RPAREN);
    
    auto whileNode = attachLoc(std::make_unique<WhileNode>(std::move(cond)), startTok);
    whileNode->body = parseBlock();
    return whileNode;
}

std::unique_ptr<ForNode> Parser::parseForStatement() {
    Token startTok = currentToken();
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
            init = attachLoc(std::make_unique<VarDeclNode>(typeStr, name, std::move(expr)), startTok);
        } else {
            // Assignment: identifier = expr
            std::string name = currentToken().value;
            advance();
            expect(TokenType::TOK_EQUALS);
            auto expr = parseExpression();
            init = attachLoc(std::make_unique<VarAssignNode>(name, std::move(expr)), startTok);
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
        update = attachLoc(std::make_unique<VarAssignNode>(name, std::move(expr)), startTok);
    }
    expect(TokenType::TOK_RPAREN);
    
    auto forNode = attachLoc(std::make_unique<ForNode>(std::move(init), std::move(cond), std::move(update)), startTok);
    forNode->body = parseBlock();
    return forNode;
}

std::unique_ptr<ReturnNode> Parser::parseReturnStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_RETURN);
    std::unique_ptr<ASTNode> expr = nullptr;
    if (currentToken().type != TokenType::TOK_SEMICOLON) {
        expr = parseExpression();
    }
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<ReturnNode>(std::move(expr)), startTok);
}

std::unique_ptr<AssertNode> Parser::parseAssertStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_ASSERT);
    expect(TokenType::TOK_LPAREN);
    auto cond = parseExpression();
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<AssertNode>(std::move(cond)), startTok);
}

std::unique_ptr<TryCatchNode> Parser::parseTryCatchStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_TRY);
    auto try_body = parseBlock();
    
    expect(TokenType::TOK_CATCH);
    expect(TokenType::TOK_LPAREN);
    std::string catch_type = parseTypeString();
    std::string catch_name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_RPAREN);
    
    auto catch_body = parseBlock();
    
    return attachLoc(std::make_unique<TryCatchNode>(std::move(try_body), catch_type, catch_name, std::move(catch_body)), startTok);
}

std::unique_ptr<ThrowNode> Parser::parseThrowStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_THROW);
    auto expr = parseExpression();
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<ThrowNode>(std::move(expr)), startTok);
}

std::unique_ptr<EffectDeclNode> Parser::parseEffectDecl() {
    Token startTok = currentToken();
    expect(TokenType::TOK_EFFECT);
    std::string name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    
    auto effect = attachLoc(std::make_unique<EffectDeclNode>(name), startTok);
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
        
        auto routine = attachLoc(std::make_unique<RoutineNode>(mName, std::nullopt, false), startTok);
        for (const auto& p : params) routine->params.push_back({p.first, p.second});
        routine->returnType = retType;
        // No body
        effect->methods.push_back(std::move(routine));
    }
    expect(TokenType::TOK_RBRACE);
    return effect;
}

std::unique_ptr<HandleNode> Parser::parseHandleStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_HANDLE);
    std::string effect_name = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    
    auto handle = attachLoc(std::make_unique<HandleNode>(effect_name), startTok);
    
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
    Token startTok = currentToken();
    expect(TokenType::TOK_YIELD);
    std::string eName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_DOT);
    std::string mName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LPAREN);
    
    auto yieldNode = attachLoc(std::make_unique<YieldNode>(eName, mName), startTok);
    
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        yieldNode->args.push_back(parseExpression());
        if (currentToken().type == TokenType::TOK_COMMA) advance();
    }
    expect(TokenType::TOK_RPAREN);
    
    return yieldNode;
}

std::unique_ptr<ResumeNode> Parser::parseResumeStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_RESUME);
    expect(TokenType::TOK_LPAREN);
    auto expr = parseExpression();
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<ResumeNode>(std::move(expr)), startTok);
}

std::unique_ptr<FuncCallNode> Parser::parseFuncCall(std::string name) {
    Token startTok = currentToken();
    expect(TokenType::TOK_LPAREN);
    std::vector<std::unique_ptr<ASTNode>> args;
    
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        args.push_back(parseExpression());
        if (currentToken().type == TokenType::TOK_COMMA) {
            advance();
        }
    }
    expect(TokenType::TOK_RPAREN);
    
    return attachLoc(std::make_unique<FuncCallNode>(name, std::move(args)), startTok);
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    Token startTok = currentToken();
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
    } else if (currentToken().type == TokenType::TOK_ASSERT) {
        return parseAssertStatement();
    } else if (currentToken().type == TokenType::TOK_FREE) {
        return parseFreeStatement();
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
        return attachLoc(std::make_unique<DerefAssignNode>(std::move(target), std::move(val)), startTok);
    } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
        // Could be a function call statement, assignment, member assignment, or custom type variable declaration
        std::string name = parseQualifiedName();
        
        if (currentToken().type == TokenType::TOK_LESS_THAN) {
            std::string typeStr = name;
            std::vector<std::string> parsed_types;
            typeStr += "<";
            advance();
            while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
                std::string ts = parseTypeString();
                parsed_types.push_back(ts);
                typeStr += ts;
                if (currentToken().type == TokenType::TOK_COMMA) {
                    typeStr += ",";
                    advance();
                }
            }
            typeStr += ">";
            expect(TokenType::TOK_GREATER_THAN);
            
            if (currentToken().type == TokenType::TOK_LPAREN) {
                auto call = parseFuncCall(name);
                call->type_args = parsed_types;
                expect(TokenType::TOK_SEMICOLON);
                return call;
            }
            
            while (currentToken().type == TokenType::TOK_STAR || currentToken().type == TokenType::TOK_LBRACKET) {
                if (currentToken().type == TokenType::TOK_STAR) {
                    typeStr += "*";
                    advance();
                } else if (currentToken().type == TokenType::TOK_LBRACKET) {
                    if (pos + 1 < tokens.size() && tokens[pos + 1].type == TokenType::TOK_RBRACKET) {
                        advance();
                        expect(TokenType::TOK_RBRACKET);
                        typeStr += "[]";
                    } else {
                        break;
                    }
                }
            }
            
            std::string varName = currentToken().value;
            expect(TokenType::TOK_IDENTIFIER);
            
            if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto sizeExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                expect(TokenType::TOK_SEMICOLON);
                return attachLoc(std::make_unique<ArrayDeclNode>(typeStr, varName, std::move(sizeExpr)), startTok);
            }
            
            std::unique_ptr<ASTNode> expr = nullptr;
            if (currentToken().type == TokenType::TOK_EQUALS) {
                advance();
                expr = parseExpression();
            }
            expect(TokenType::TOK_SEMICOLON);
            return attachLoc(std::make_unique<VarDeclNode>(typeStr, varName, std::move(expr)), startTok);
        } else if (currentToken().type == TokenType::TOK_IDENTIFIER) {
            // It's a variable declaration: CustomType varName;
            std::string varName = currentToken().value;
            advance();
            
            if (currentToken().type == TokenType::TOK_LBRACKET) {
                advance();
                auto sizeExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                expect(TokenType::TOK_SEMICOLON);
                return attachLoc(std::make_unique<ArrayDeclNode>(name, varName, std::move(sizeExpr)), startTok);
            }
            
            std::unique_ptr<ASTNode> expr = nullptr;
            if (currentToken().type == TokenType::TOK_EQUALS) {
                advance();
                expr = parseExpression();
            }
            expect(TokenType::TOK_SEMICOLON);
            return attachLoc(std::make_unique<VarDeclNode>(name, varName, std::move(expr)), startTok);
        } else if (currentToken().type == TokenType::TOK_LBRACKET) {
            std::unique_ptr<ASTNode> arrayExpr = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
            while (true) {
                advance(); // Consume '['
                auto idxExpr = parseExpression();
                expect(TokenType::TOK_RBRACKET);
                
                if (currentToken().type == TokenType::TOK_LBRACKET) {
                    arrayExpr = attachLoc(std::make_unique<ArrayIndexNode>(std::move(arrayExpr), std::move(idxExpr)), startTok);
                } else {
                    expect(TokenType::TOK_EQUALS);
                    auto valExpr = parseExpression();
                    expect(TokenType::TOK_SEMICOLON);
                    return attachLoc(std::make_unique<ArrayAssignNode>(std::move(arrayExpr), std::move(idxExpr), std::move(valExpr)), startTok);
                }
            }
        } else if (currentToken().type == TokenType::TOK_EQUALS) {
            advance(); // Consume '='
            auto expr = parseExpression();
            expect(TokenType::TOK_SEMICOLON);
            return attachLoc(std::make_unique<VarAssignNode>(name, std::move(expr)), startTok);
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
                auto obj = attachLoc(std::make_unique<VarAccessNode>(name), startTok);
                return attachLoc(std::make_unique<MethodCallNode>(std::move(obj), field, std::move(args)), startTok);
            } else {
                expect(TokenType::TOK_EQUALS);
                auto expr = parseExpression();
                expect(TokenType::TOK_SEMICOLON);
                return attachLoc(std::make_unique<MemberAssignNode>(name, field, std::move(expr)), startTok);
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

std::unique_ptr<RoutineNode> Parser::parseRoutine(bool isExported) {
    Token startTok = currentToken();
    expect(TokenType::TOK_ROUTINE);
    
    std::optional<Receiver> receiver = std::nullopt;
    
    // Check for receiver
    if (currentToken().type == TokenType::TOK_LPAREN) {
        advance();
        std::string rType = parseTypeString();
        std::string rName = currentToken().value;
        expect(TokenType::TOK_IDENTIFIER);
        bool isPtr = false;
        // Pointers are handled by parseTypeString usually, but maybe they did:
        // routine (File* f)? If so parseTypeString returns "File*".
        // Let's keep it clean since parseTypeString handles pointer types.
        expect(TokenType::TOK_RPAREN);
        receiver = Receiver{rName, rType, isPtr};
    }
    
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error(ErrorReporter::formatError("Expected routine name", filename, currentToken().line, currentToken().col));
    }
    std::string name = currentToken().value;
    advance();
    
    std::vector<std::string> type_params;
    if (currentToken().type == TokenType::TOK_LESS_THAN) {
        advance();
        while (currentToken().type != TokenType::TOK_GREATER_THAN && currentToken().type != TokenType::TOK_EOF) {
            type_params.push_back(currentToken().value);
            expect(TokenType::TOK_IDENTIFIER);
            if (currentToken().type == TokenType::TOK_COMMA) advance();
        }
        expect(TokenType::TOK_GREATER_THAN);
    }
    
    if (receiver) {
        name = receiver->type + "_" + name;
    }
    
    auto routine = attachLoc(std::make_unique<RoutineNode>(name, receiver, isExported), startTok);
    routine->type_params = type_params;
    
    if (receiver) {
        std::string pType = receiver->type;
        if (receiver->isPointer) pType += "*";
        routine->params.push_back({pType, receiver->name});
    }
    
    expect(TokenType::TOK_LPAREN);
    while (currentToken().type != TokenType::TOK_RPAREN && currentToken().type != TokenType::TOK_EOF) {
        std::string pType = parseTypeString();
        // pointers and arrays are now handled by parseTypeString, so we don't need the inner while loop here
        std::string pName = currentToken().value;
        advance();
        routine->params.push_back({pType, pName});
        
        if (currentToken().type == TokenType::TOK_COMMA) advance();
    }
    expect(TokenType::TOK_RPAREN);
    
    if (currentToken().type == TokenType::TOK_ARROW) {
        advance();
        routine->returnType = parseTypeString();
    }
    
    routine->body = parseBlock();
    
    return routine;
}

std::unique_ptr<ExternRoutineNode> Parser::parseExternRoutine() {
    Token startTok = currentToken();
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
        std::string type = parseTypeString();
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
        returnType = parseTypeString();
    }
    expect(TokenType::TOK_SEMICOLON);
    
    return attachLoc(std::make_unique<ExternRoutineNode>(name, params, isVariadic, returnType), startTok);
}

std::unique_ptr<StructDefNode> Parser::parseStructDef(bool isExported) {
    Token startTok = currentToken();
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
    return attachLoc(std::make_unique<StructDefNode>(name, type_params, fields, isExported), startTok);
}

std::unique_ptr<ImportNode> Parser::parseImport() {
    Token startTok = currentToken();
    expect(TokenType::TOK_IMPORT);
    
    if (currentToken().type == TokenType::TOK_STRING) {
        // Legacy: import "file.alu";
        std::string moduleName = currentToken().value;
        advance();
        
        std::string alias = "";
        if (currentToken().type == TokenType::TOK_AS) {
            advance(); // consume 'as'
            if (currentToken().type != TokenType::TOK_IDENTIFIER) {
                throw std::runtime_error(ErrorReporter::formatError("Expected identifier after 'as' in import", filename, currentToken().line, currentToken().col));
            }
            alias = currentToken().value;
            advance();
        }
        
        expect(TokenType::TOK_SEMICOLON);
        return attachLoc(std::make_unique<ImportNode>(moduleName, false, alias), startTok);
    }
    
    // New module-path syntax: import std::fs;
    // Parse qualified name: ident (:: ident)*
    if (currentToken().type != TokenType::TOK_IDENTIFIER) {
        throw std::runtime_error(ErrorReporter::formatError("Expected module path or string literal after 'import'", filename, currentToken().line, currentToken().col));
    }
    
    std::string modulePath = currentToken().value;
    advance();
    
    while (currentToken().type == TokenType::TOK_DOUBLE_COLON) {
        advance(); // consume ::
        if (currentToken().type != TokenType::TOK_IDENTIFIER) {
            throw std::runtime_error(ErrorReporter::formatError("Expected identifier after '::' in import path", filename, currentToken().line, currentToken().col));
        }
        modulePath += "::" + currentToken().value;
        advance();
    }
    
    std::string alias = "";
    if (currentToken().type == TokenType::TOK_AS) {
        advance(); // consume 'as'
        if (currentToken().type != TokenType::TOK_IDENTIFIER) {
            throw std::runtime_error(ErrorReporter::formatError("Expected identifier after 'as' in import", filename, currentToken().line, currentToken().col));
        }
        alias = currentToken().value;
        advance();
    }
    
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<ImportNode>(modulePath, true, alias), startTok);
}

std::unique_ptr<NamespaceNode> Parser::parseNamespace() {
    Token startTok = currentToken();
    expect(TokenType::TOK_NAMESPACE);
    std::string nsName = currentToken().value;
    expect(TokenType::TOK_IDENTIFIER);
    expect(TokenType::TOK_LBRACE);
    
    auto nsNode = attachLoc(std::make_unique<NamespaceNode>(nsName), startTok);
    
    while (currentToken().type != TokenType::TOK_RBRACE && currentToken().type != TokenType::TOK_EOF) {
        // Recursive parsing of top level items inside a namespace
        std::vector<std::unique_ptr<ASTNode>> reqs, ens;
        while (currentToken().type == TokenType::TOK_REQUIRES || currentToken().type == TokenType::TOK_ENSURES) {
            bool is_req = (currentToken().type == TokenType::TOK_REQUIRES);
            advance();
            bool has_paren = false;
            if (currentToken().type == TokenType::TOK_LPAREN) {
                has_paren = true;
                advance();
            }
            auto expr = parseExpression();
            if (has_paren) {
                expect(TokenType::TOK_RPAREN);
            }
            if (is_req) reqs.push_back(std::move(expr));
            else ens.push_back(std::move(expr));
        }

        if (currentToken().type == TokenType::TOK_EXPORT) {
            advance();
            if (currentToken().type == TokenType::TOK_STRUCT) {
                if (!reqs.empty() || !ens.empty()) {
                    throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not structs", filename, currentToken().line, currentToken().col));
                }
                auto sd = parseStructDef(true);
                nsNode->declarations.push_back(std::move(sd));
            } else {
                auto rt = parseRoutine(true);
                rt->requires_annotations = std::move(reqs);
                rt->ensures_annotations = std::move(ens);
                nsNode->declarations.push_back(std::move(rt));
            }
        } else if (currentToken().type == TokenType::TOK_ROUTINE) {
            auto rt = parseRoutine(false);
            rt->requires_annotations = std::move(reqs);
            rt->ensures_annotations = std::move(ens);
            nsNode->declarations.push_back(std::move(rt));
        } else if (currentToken().type == TokenType::TOK_EXTERN) {
            auto ext = parseExternRoutine();
            ext->requires_annotations = std::move(reqs);
            ext->ensures_annotations = std::move(ens);
            nsNode->declarations.push_back(std::move(ext));
        } else if (currentToken().type == TokenType::TOK_STRUCT) {
            if (!reqs.empty() || !ens.empty()) {
                throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not structs", filename, currentToken().line, currentToken().col));
            }
            auto sd = parseStructDef(false);
            nsNode->declarations.push_back(std::move(sd));
        } else if (currentToken().type == TokenType::TOK_EFFECT) {
            if (!reqs.empty() || !ens.empty()) {
                throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not effects", filename, currentToken().line, currentToken().col));
            }
            nsNode->declarations.push_back(parseEffectDecl());
        } else if (currentToken().type == TokenType::TOK_NAMESPACE) {
            nsNode->declarations.push_back(parseNamespace());
        } else {
            throw std::runtime_error(ErrorReporter::formatError("Unexpected token in namespace: " + currentToken().value, filename, currentToken().line, currentToken().col));
        }
    }
    
    expect(TokenType::TOK_RBRACE);
    return nsNode;
}



std::unique_ptr<ProgramNode> Parser::parse() {
    Token startTok = currentToken();
    auto program = attachLoc(std::make_unique<ProgramNode>(), startTok);
    
    while (currentToken().type != TokenType::TOK_EOF) {
        std::vector<std::unique_ptr<ASTNode>> reqs, ens;
        while (currentToken().type == TokenType::TOK_REQUIRES || currentToken().type == TokenType::TOK_ENSURES) {
            bool is_req = (currentToken().type == TokenType::TOK_REQUIRES);
            advance();
            bool has_paren = false;
            if (currentToken().type == TokenType::TOK_LPAREN) {
                has_paren = true;
                advance();
            }
            auto expr = parseExpression();
            if (has_paren) {
                expect(TokenType::TOK_RPAREN);
            }
            if (is_req) reqs.push_back(std::move(expr));
            else ens.push_back(std::move(expr));
        }

        if (currentToken().type == TokenType::TOK_EXPORT) {
            advance();
            if (currentToken().type == TokenType::TOK_STRUCT) {
                if (!reqs.empty() || !ens.empty()) {
                    throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not structs", filename, currentToken().line, currentToken().col));
                }
                auto sd = parseStructDef(true);
                program->declarations.push_back(std::move(sd));
            } else {
                auto rt = parseRoutine(true);
                rt->requires_annotations = std::move(reqs);
                rt->ensures_annotations = std::move(ens);
                program->declarations.push_back(std::move(rt));
            }
        } else if (currentToken().type == TokenType::TOK_ROUTINE) {
            auto rt = parseRoutine(false);
            rt->requires_annotations = std::move(reqs);
            rt->ensures_annotations = std::move(ens);
            program->declarations.push_back(std::move(rt));
        } else if (currentToken().type == TokenType::TOK_EXTERN) {
            auto ext = parseExternRoutine();
            ext->requires_annotations = std::move(reqs);
            ext->ensures_annotations = std::move(ens);
            program->declarations.push_back(std::move(ext));
        } else if (currentToken().type == TokenType::TOK_STRUCT) {
            if (!reqs.empty() || !ens.empty()) {
                throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not structs", filename, currentToken().line, currentToken().col));
            }
            auto sd = parseStructDef(false);
            program->declarations.push_back(std::move(sd));
        } else if (currentToken().type == TokenType::TOK_IMPORT) {
            if (!reqs.empty() || !ens.empty()) {
                throw std::runtime_error(ErrorReporter::formatError("Annotations @requires/@ensures are only allowed on routines, not imports", filename, currentToken().line, currentToken().col));
            }
            program->declarations.push_back(parseImport());
        } else if (currentToken().type == TokenType::TOK_NAMESPACE) {
            program->declarations.push_back(parseNamespace());
        } else if (currentToken().type == TokenType::TOK_EFFECT) {
            program->declarations.push_back(parseEffectDecl());
        } else {
            throw std::runtime_error(ErrorReporter::formatError("Unexpected token at top level: " + currentToken().value, filename, currentToken().line, currentToken().col));
        }
    }
    
    return program;
}

std::unique_ptr<FreeNode> Parser::parseFreeStatement() {
    Token startTok = currentToken();
    expect(TokenType::TOK_FREE);
    expect(TokenType::TOK_LPAREN);
    auto expr = parseExpression();
    expect(TokenType::TOK_RPAREN);
    expect(TokenType::TOK_SEMICOLON);
    return attachLoc(std::make_unique<FreeNode>(std::move(expr)), startTok);
}
