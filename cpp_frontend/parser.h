#pragma once
#include "lexer.h"
#include "ast.h"

class Parser {
private:
    std::vector<Token> tokens;
    size_t pos;
    Token currentToken();
    void advance();
    void expect(TokenType type);
    
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<UnsafeBlockNode> parseUnsafeBlock();
    std::unique_ptr<AsmCallNode> parseAsmCall();
    std::unique_ptr<RoutineNode> parseRoutine();
public:
    Parser(std::vector<Token> toks);
    std::unique_ptr<ProgramNode> parse();
};
