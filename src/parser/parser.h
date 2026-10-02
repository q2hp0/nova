#pragma once

#include "../lexer/lexer.h"
#include "ast.h"

#include <memory>
#include <string>
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::vector<std::unique_ptr<Stmt>> parse();

    bool holdsOnlyAnExpression();
    bool usedEveryToken();

private:
    std::unique_ptr<Stmt> declaration();
    std::unique_ptr<Stmt> functionDeclaration();
    std::vector<std::string> parameterList();
    std::unique_ptr<Stmt> letDeclaration();
    std::unique_ptr<BlockStmt> braceBlock();
    std::unique_ptr<BlockStmt> singleLineBody();
    std::unique_ptr<Expr> newExpression();
    std::unique_ptr<Expr> superExpression();

    std::unique_ptr<Stmt> classStatement();
    std::unique_ptr<Stmt> tryStatement();
    std::unique_ptr<Stmt> throwStatement();
    std::unique_ptr<Stmt> importStatement();
    std::unique_ptr<Stmt> exportStatement();

    std::unique_ptr<Pattern> patternNode();
    std::unique_ptr<Pattern> namePattern(const std::string& name);
    std::unique_ptr<Pattern> nameOrNestedPattern();
    std::unique_ptr<Pattern> listPattern();
    std::unique_ptr<Pattern> mapPattern();
    void takeName(const std::string& name);
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> forStatement();
    std::unique_ptr<Stmt> forInStatement();
    std::unique_ptr<Stmt> switchStatement();
    std::unique_ptr<BlockStmt> caseBody(const Token& label);
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> expressionStatement();
    std::unique_ptr<BlockStmt> blockStatement();

    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> condition();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> orExpr();
    std::unique_ptr<Expr> andExpr();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> call();
    std::unique_ptr<Expr> primary();
    std::unique_ptr<Expr> functionExpression();
    std::unique_ptr<Expr> listLiteral();
    std::unique_ptr<Expr> mapLiteral();
    std::unique_ptr<Expr> interpolated(const Token& token);

    bool match(TokenType type);
    bool matchEither(TokenType first, TokenType second);
    bool startsForIn() const;
    bool matchCompoundOperator();
    bool check(TokenType type) const;
    bool isAtEnd() const;

    const Token& advance();
    const Token& peek() const;
    const Token& peekNext() const;
    const Token& tokenAt(size_t offset) const;
    const Token& previous() const;

    const Token& consume(
        TokenType type,
        const std::string& message
    );

    [[noreturn]] void fail(
        const Token& token,
        const std::string& message
    ) const;

    void requireEndOfStatement();

    std::vector<Token> tokens;
    std::vector<std::string> patternNames;

    void skipToken(TokenType type);
    size_t current = 0;
};

std::string compoundSymbol(TokenType type);
