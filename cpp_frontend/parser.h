#pragma once
#include "lexer.h"
#include "ast.h"

class Parser {
private:
    std::vector<Token> tokens;
    size_t pos;
    std::string filename;
    
    Token currentToken();
    void advance();
    void expect(TokenType type);

    template<typename T>
    std::unique_ptr<T> attachLoc(std::unique_ptr<T> node, const Token& tok) {
        node->line = tok.line;
        node->col = tok.col;
        node->file = this->filename;
        return node;
    }
    
    std::unique_ptr<AsmCallNode> parseAsmCall();
    std::unique_ptr<UnsafeBlockNode> parseUnsafeBlock();
    std::unique_ptr<IfNode> parseIfStatement();
    std::unique_ptr<WhileNode> parseWhileStatement();
    std::unique_ptr<ForNode> parseForStatement();
    std::unique_ptr<ReturnNode> parseReturnStatement();
    std::unique_ptr<AssertNode> parseAssertStatement();
    std::unique_ptr<FreeNode> parseFreeStatement();
    std::unique_ptr<TryCatchNode> parseTryCatchStatement();
    std::unique_ptr<ThrowNode> parseThrowStatement();
    std::unique_ptr<EffectDeclNode> parseEffectDecl();
    std::unique_ptr<HandleNode> parseHandleStatement();
    std::unique_ptr<YieldNode> parseYieldStatement();
    std::unique_ptr<ResumeNode> parseResumeStatement();
    std::unique_ptr<FuncCallNode> parseFuncCall(std::string name);
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseVarDecl();
    std::string parseTypeString();
    std::unique_ptr<ASTNode> parseStatement();
    std::vector<std::unique_ptr<ASTNode>> parseBlock();
    std::unique_ptr<RoutineNode> parseRoutine(bool isExported);
    std::unique_ptr<ExternRoutineNode> parseExternRoutine();
    std::unique_ptr<StructDefNode> parseStructDef(bool isExported);
    std::unique_ptr<ImportNode> parseImport();
    std::unique_ptr<NamespaceNode> parseNamespace();
    std::string parseQualifiedName();
    
public:
    Parser(std::vector<Token> toks, std::string fname = "unknown");
    std::unique_ptr<ProgramNode> parse();
};
