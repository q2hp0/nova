#include "parser.h"

#include "../errors/errors.h"
#include "../lexer/lexer.h"
#include "../runtime/value.h"

std::string compoundSymbol(TokenType type) {
    switch (type) {
        case TokenType::PlusEqual: return "+";
        case TokenType::MinusEqual: return "-";
        case TokenType::StarEqual: return "*";
        case TokenType::SlashEqual: return "/";
        case TokenType::PercentEqual: return "%";
        default: return "+";
    }
}

Parser::Parser(std::vector<Token> tokens)
    : tokens(std::move(tokens)) {
}

bool Parser::usedEveryToken() {
    return isAtEnd();
}

bool Parser::holdsOnlyAnExpression() {
    try {
        expression();
    } catch (const NovaError&) {
        return false;
    }

    return usedEveryToken();
}

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;

    while (!isAtEnd()) {
        statements.push_back(declaration());
    }

    return statements;
}

std::unique_ptr<Stmt> Parser::declaration() {
    if (match(TokenType::Function)) {
        return functionDeclaration();
    }

    if (check(TokenType::Let)) {
        advance();
        return letDeclaration();
    }

    return statement();
}

std::vector<std::string> Parser::parameterList() {
    consume(
        TokenType::LeftParen,
        "expected '(' before the parameter list"
    );

    std::vector<std::string> parameters;

    if (!check(TokenType::RightParen)) {
        do {
            const Token parameter = consume(
                TokenType::Identifier,
                "expected a parameter name"
            );

            for (const auto& existing : parameters) {
                if (existing == parameter.text) {
                    fail(
                        parameter,
                        "duplicate parameter name '" +
                        parameter.text + "'"
                    );
                }
            }

            parameters.push_back(parameter.text);
        } while (match(TokenType::Comma));
    }

    consume(
        TokenType::RightParen,
        "expected ')' after the parameter list"
    );

    return parameters;
}

std::unique_ptr<Stmt> Parser::functionDeclaration() {
    const Token name = consume(
        TokenType::Identifier,
        "expected a function name after 'function'"
    );

    std::vector<std::string> parameters = parameterList();

    consume(
        TokenType::LeftBrace,
        "expected '{' before the body of '" + name.text + "'"
    );

    auto body = blockStatement();

    if (body->statements.empty()) {
        fail(name, "function '" + name.text + "' has an empty body");
    }

    return std::make_unique<FunctionStmt>(
        name.line,
        name.text,
        std::move(parameters),
        std::move(body)
    );
}

std::unique_ptr<Stmt> Parser::letDeclaration() {
    const Token first = peek();

    if (check(TokenType::LeftBracket) || check(TokenType::LeftBrace)) {
        patternNames.clear();

        std::unique_ptr<Pattern> pattern = patternNode();

        consume(
            TokenType::Equal,
            "a pattern needs a value, write '=' after it"
        );

        std::unique_ptr<Expr> initializer = expression();

        requireEndOfStatement();

        return std::make_unique<DestructureStmt>(
            first.line,
            std::move(pattern),
            std::move(initializer)
        );
    }

    const Token name = consume(
        TokenType::Identifier,
        "expected a variable name after 'let'"
    );

    std::unique_ptr<Expr> initializer;

    if (match(TokenType::Equal)) {
        initializer = expression();
    }

    requireEndOfStatement();

    return std::make_unique<LetStmt>(
        name.line,
        name.text,
        std::move(initializer)
    );
}

std::unique_ptr<Pattern> Parser::patternNode() {
    if (match(TokenType::LeftBracket)) {
        return listPattern();
    }

    if (match(TokenType::LeftBrace)) {
        return mapPattern();
    }

    const Token token = consume(
        TokenType::Identifier,
        "expected a variable name inside the pattern"
    );

    return namePattern(token.text);
}

std::unique_ptr<Pattern> Parser::listPattern() {
    auto pattern = std::make_unique<Pattern>();
    pattern->kind = PatternKind::List;

    int position = 0;

    while (!check(TokenType::RightBracket) && !isAtEnd()) {
        if (match(TokenType::Rest)) {
            const Token dots = previous();

            if (position == 0) {
                fail(dots, "put at least one name before '...'");
            }

            const Token name = consume(
                TokenType::Identifier,
                "expected a name after '...'"
            );

            pattern->rest = position;
            pattern->parts.push_back(namePattern(name.text));
            pattern->parts.back()->position = position;

            if (!match(TokenType::Comma)) {
                break;
            }

            continue;
        }

        if (check(TokenType::Comma) &&
            peekNext().type != TokenType::RightBracket) {
            skipToken(TokenType::Comma);
            position++;
            continue;
        }

        if (check(TokenType::Identifier) && peek().text == "_" &&
            (peekNext().type == TokenType::Comma ||
             peekNext().type == TokenType::RightBracket)) {
            skipToken(TokenType::Identifier);

            if (!match(TokenType::Comma)) {
                break;
            }

            position++;
            continue;
        }

        auto part = patternNode();
        part->position = position;
        pattern->parts.push_back(std::move(part));

        if (!match(TokenType::Comma)) {
            break;
        }

        position++;
    }

    consume(
        TokenType::RightBracket,
        "expected ']' to close the pattern"
    );

    if (pattern->parts.empty()) {
        fail(previous(), "this pattern holds no names");
    }

    return pattern;
}

std::unique_ptr<Pattern> Parser::mapPattern() {
    auto pattern = std::make_unique<Pattern>();
    pattern->kind = PatternKind::Map;

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        std::string key;

        if (match(TokenType::String)) {
            key = previous().text;
        } else if (match(TokenType::Number)) {
            key = formatNumber(previous().number);
        } else {
            const Token token = consume(
                TokenType::Identifier,
                "expected a key inside the pattern"
            );

            key = token.text;
        }

        if (match(TokenType::Colon) ||
            check(TokenType::LeftBracket) ||
            check(TokenType::LeftBrace)) {
            if (match(TokenType::Colon)) {
                pattern->parts.push_back(nameOrNestedPattern());
            } else {
                pattern->parts.push_back(patternNode());
            }
        } else {
            pattern->parts.push_back(namePattern(key));
        }

        pattern->parts.back()->key = key;

        if (!match(TokenType::Comma)) {
            break;
        }
    }

    consume(
        TokenType::RightBrace,
        "expected '}' to close the pattern"
    );

    if (pattern->parts.empty()) {
        fail(previous(), "this pattern holds no names");
    }

    return pattern;
}

std::unique_ptr<Pattern> Parser::namePattern(
    const std::string& name
) {
    auto pattern = std::make_unique<Pattern>();
    pattern->kind = PatternKind::Name;
    pattern->name = name;

    takeName(name);

    return pattern;
}

std::unique_ptr<Pattern> Parser::nameOrNestedPattern() {
    if (check(TokenType::LeftBracket) || check(TokenType::LeftBrace)) {
        return patternNode();
    }

    const Token name = consume(
        TokenType::Identifier,
        "expected a variable name after ':'"
    );

    return namePattern(name.text);
}

void Parser::takeName(const std::string& name) {
    if (name == "_") {
        return;
    }

    for (const std::string& seen : patternNames) {
        if (seen == name) {
            fail(
                previous(),
                "this name is already in the pattern"
            );
        }
    }

    patternNames.push_back(name);
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match(TokenType::If)) {
        return ifStatement();
    }

    if (match(TokenType::While)) {
        return whileStatement();
    }

    if (match(TokenType::For)) {
        if (startsForIn()) {
            advance();
            advance();
            return forInStatement();
        }

        return forStatement();
    }

    if (match(TokenType::Switch)) {
        return switchStatement();
    }

    if (match(TokenType::Class)) {
        return classStatement();
    }

    if (match(TokenType::Try)) {
        return tryStatement();
    }

    if (match(TokenType::Throw)) {
        return throwStatement();
    }

    if (match(TokenType::Import)) {
        return importStatement();
    }

    if (match(TokenType::Export)) {
        return exportStatement();
    }

    if (match(TokenType::Return)) {
        return returnStatement();
    }

    if (match(TokenType::Break)) {
        const Token keyword = previous();
        requireEndOfStatement();

        return std::make_unique<BreakStmt>(keyword.line);
    }

    if (match(TokenType::Continue)) {
        const Token keyword = previous();
        requireEndOfStatement();

        return std::make_unique<ContinueStmt>(keyword.line);
    }

    if (match(TokenType::LeftBrace)) {
        return blockStatement();
    }

    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::classStatement() {
    const Token keyword = previous();

    const Token name = consume(
        TokenType::Identifier,
        "expected a class name after 'class'"
    );

    std::string baseName;

    if (match(TokenType::Extends)) {
        baseName = consume(
            TokenType::Identifier,
            "expected a base class name after 'extends'"
        ).text;
    }

    consume(
        TokenType::LeftBrace,
        "expected '{' to open the class body"
    );

    auto node = std::make_unique<ClassStmt>(
        keyword.line,
        name.text,
        baseName
    );

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        bool isStatic = match(TokenType::Static);

        const Token methodName = consume(
            TokenType::Identifier,
            "expected a field or method name in the class body"
        );

        std::vector<std::string> parameters;

        if (match(TokenType::LeftParen)) {
            if (!check(TokenType::RightParen)) {
                do {
                    parameters.push_back(
                        consume(
                            TokenType::Identifier,
                            "expected a parameter name"
                        ).text
                    );
                } while (match(TokenType::Comma));
            }

            consume(
                TokenType::RightParen,
                "expected ')' after the parameter list"
            );

            std::unique_ptr<BlockStmt> body;

            if (match(TokenType::LeftBrace)) {
                body = blockStatement();
            } else {
                body = singleLineBody();
            }

            ClassMethod method;
            method.name = methodName.text;
            method.parameters = std::move(parameters);
            method.body = std::move(body);
            method.isStatic = isStatic;

            node->methods.push_back(std::move(method));

            continue;
        }

        if (isStatic) {
            fail(methodName, "'static' only works on a method");
        }

        for (const std::string& seen : node->fields) {
            if (seen == methodName.text) {
                fail(methodName, "this field is already in the class");
            }
        }

        node->fields.push_back(methodName.text);

        if (match(TokenType::Equal)) {
            node->fieldDefaults.push_back(expression());
        } else {
            node->fieldDefaults.push_back(nullptr);
        }

        if (match(TokenType::Comma)) {
            continue;
        }

        if (check(TokenType::RightBrace)) {
            break;
        }

        requireEndOfStatement();
    }

    consume(
        TokenType::RightBrace,
        "expected '}' to close the class body"
    );

    return node;
}

std::unique_ptr<Stmt> Parser::tryStatement() {
    const Token keyword = previous();

    auto node = std::make_unique<TryStmt>(
        keyword.line,
        braceBlock()
    );

    if (match(TokenType::Catch)) {
        if (match(TokenType::LeftParen)) {
            node->caught = consume(
                TokenType::Identifier,
                "expected a name to catch into"
            ).text;

            consume(
                TokenType::RightParen,
                "expected ')' after the caught name"
            );
        } else if (check(TokenType::Identifier)) {
            node->caught = advance().text;
        }

        node->handler = braceBlock();
    }

    if (match(TokenType::Finally)) {
        node->cleanup = braceBlock();
    }

    if (!node->handler && !node->cleanup) {
        fail(keyword, "'try' needs a 'catch' or a 'finally'");
    }

    return node;
}

std::unique_ptr<Stmt> Parser::throwStatement() {
    const Token keyword = previous();

    std::unique_ptr<Expr> value;

    if (!check(TokenType::Semicolon) && !check(TokenType::RightBrace)) {
        value = expression();
    }

    requireEndOfStatement();

    return std::make_unique<ThrowStmt>(keyword.line, std::move(value));
}

std::unique_ptr<Stmt> Parser::importStatement() {
    const Token keyword = previous();

    const Token path = consume(
        TokenType::String,
        "expected a path in quotes after 'import'"
    );

    if (path.text.find(INTERPOLATE_OPEN) != std::string::npos) {
        fail(path, "an import path cannot hold a value");
    }

    if (match(TokenType::From)) {
        fail(path, "'from' does not follow 'import'");
    }

    auto node = std::make_unique<ImportStmt>(keyword.line, path.text);

    if (match(TokenType::As)) {
        node->alias = consume(
            TokenType::Identifier,
            "expected a name after 'as'"
        ).text;
    }

    requireEndOfStatement();

    return node;
}

std::unique_ptr<Stmt> Parser::exportStatement() {
    const Token keyword = previous();

    std::vector<std::string> names;

    if (match(TokenType::LeftBrace)) {
        if (!check(TokenType::RightBrace)) {
            do {
                names.push_back(
                    consume(
                        TokenType::Identifier,
                        "expected a name to export"
                    ).text
                );
            } while (match(TokenType::Comma));
        }

        consume(
            TokenType::RightBrace,
            "expected '}' after the export list"
        );
    } else {
        names.push_back(
            consume(
                TokenType::Identifier,
                "expected a name after 'export'"
            ).text
        );

        while (match(TokenType::Comma)) {
            names.push_back(
                consume(
                    TokenType::Identifier,
                    "expected a name after ','"
                ).text
            );
        }
    }

    requireEndOfStatement();

    if (names.empty()) {
        fail(keyword, "'export' needs at least one name");
    }

    return std::make_unique<ExportStmt>(keyword.line, std::move(names));
}

std::unique_ptr<Stmt> Parser::ifStatement() {
    const Token keyword = previous();

    auto condition = Parser::condition();

    auto thenBranch = statement();

    std::unique_ptr<Stmt> elseBranch;

    if (match(TokenType::Else)) {
        if (match(TokenType::If)) {
            elseBranch = ifStatement();
        } else {
            elseBranch = statement();
        }
    }

    return std::make_unique<IfStmt>(
        keyword.line,
        std::move(condition),
        std::move(thenBranch),
        std::move(elseBranch)
    );
}

std::unique_ptr<Stmt> Parser::whileStatement() {
    const Token keyword = previous();

    auto condition = Parser::condition();

    auto body = statement();

    return std::make_unique<WhileStmt>(
        keyword.line,
        std::move(condition),
        std::move(body)
    );
}

std::unique_ptr<Stmt> Parser::forStatement() {
    const Token keyword = previous();

    consume(
        TokenType::LeftParen,
        "expected '(' after 'for'"
    );

    std::unique_ptr<Stmt> initializer;

    if (match(TokenType::Let)) {
        const Token name = consume(
            TokenType::Identifier,
            "expected a variable name after 'let'"
        );

        std::unique_ptr<Expr> value;

        if (match(TokenType::Equal)) {
            value = expression();
        }

        consume(
            TokenType::Semicolon,
            "expected ';' after the loop variable"
        );

        initializer = std::make_unique<LetStmt>(
            name.line,
            name.text,
            std::move(value)
        );
    } else if (!check(TokenType::Semicolon)) {
        initializer = expressionStatement();
    } else {
        advance();
    }

    std::unique_ptr<Expr> condition;

    if (!check(TokenType::Semicolon)) {
        condition = expression();
    }

    consume(
        TokenType::Semicolon,
        "expected ';' after the loop condition"
    );

    std::unique_ptr<Expr> increment;

    if (!check(TokenType::RightParen)) {
        increment = expression();
    }

    consume(
        TokenType::RightParen,
        "expected ')' after the for clauses"
    );

    auto body = statement();

    return std::make_unique<ForStmt>(
        keyword.line,
        std::move(initializer),
        std::move(condition),
        std::move(increment),
        std::move(body)
    );
}

std::unique_ptr<Stmt> Parser::forInStatement() {
    const Token keyword = previous();

    const Token name = consume(
        TokenType::Identifier,
        "expected a variable name after 'let'"
    );

    consume(
        TokenType::In,
        "expected 'in' after '" + name.text + "'"
    );

    auto sequence = expression();

    consume(
        TokenType::RightParen,
        "expected ')' after the for-in sequence"
    );

    auto body = statement();

    return std::make_unique<ForInStmt>(
        keyword.line,
        name.text,
        std::move(sequence),
        std::move(body)
    );
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    const Token keyword = previous();

    std::unique_ptr<Expr> value;

    if (!check(TokenType::Semicolon)) {
        value = expression();
    }

    requireEndOfStatement();

    return std::make_unique<ReturnStmt>(
        keyword.line,
        std::move(value)
    );
}

std::unique_ptr<BlockStmt> Parser::caseBody(
    const Token& label
) {
    if (match(TokenType::LeftBrace)) {
        return blockStatement();
    }

    auto body = std::make_unique<BlockStmt>(label.line);

    while (!check(TokenType::Case) &&
           !check(TokenType::Default) &&
           !check(TokenType::RightBrace) &&
           !isAtEnd()) {
        std::unique_ptr<Stmt> next = declaration();

        if (next == nullptr) {
            break;
        }

        body->statements.push_back(std::move(next));
    }

    return body;
}

std::unique_ptr<Stmt> Parser::switchStatement() {
    const Token keyword = previous();

    std::unique_ptr<Expr> subject = expression();

    consume(
        TokenType::LeftBrace,
        "expected '{' to open the switch body"
    );

    std::vector<SwitchCase> cases;
    bool hasDefault = false;

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        SwitchCase branch;

        if (match(TokenType::Case)) {
            const Token label = previous();

            branch.value = expression();

            consume(
                TokenType::Colon,
                "expected ':' after a case value"
            );

            branch.body = caseBody(label);
        } else if (match(TokenType::Default)) {
            const Token label = previous();

            if (hasDefault) {
                fail(label, "this switch already has a default branch");
            }

            hasDefault = true;

            consume(
                TokenType::Colon,
                "expected ':' after default"
            );

            branch.body = caseBody(label);
        } else {
            fail(peek(), "expected 'case' or 'default' inside a switch");
        }

        cases.push_back(std::move(branch));
    }

    consume(
        TokenType::RightBrace,
        "expected '}' to close the switch"
    );

    if (cases.empty()) {
        fail(keyword, "a switch needs at least one case or a default branch");
    }

    return std::make_unique<SwitchStmt>(
        keyword.line,
        std::move(subject),
        std::move(cases)
    );
}

std::unique_ptr<BlockStmt> Parser::braceBlock() {
    consume(
        TokenType::LeftBrace,
        "expected '{' to open the body"
    );

    return blockStatement();
}

std::unique_ptr<BlockStmt> Parser::singleLineBody() {
    auto block = std::make_unique<BlockStmt>(previous().line);

    block->statements.push_back(declaration());

    if (match(TokenType::Semicolon)) {
        while (!check(TokenType::RightBrace) && !isAtEnd()) {
            block->statements.push_back(declaration());
        }
    }

    return block;
}

std::unique_ptr<BlockStmt> Parser::blockStatement() {
    auto block = std::make_unique<BlockStmt>(previous().line);

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        block->statements.push_back(declaration());
    }

    consume(
        TokenType::RightBrace,
        "expected '}' to close the block"
    );

    return block;
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    const Token start = peek();

    auto value = expression();

    if (matchCompoundOperator()) {
        const Token op = previous();

        auto target = std::move(value);
        auto targetCopy = cloneExpr(target.get());

        bool valid =
            dynamic_cast<IdentifierExpr*>(target.get()) != nullptr ||
            dynamic_cast<IndexExpr*>(target.get()) != nullptr ||
            dynamic_cast<PropertyExpr*>(target.get()) != nullptr;

        if (!valid) {
            fail(
                op,
                "this expression cannot be assigned to"
            );
        }

        auto right = expression();

        auto combined = std::make_unique<BinaryExpr>(
            op.line,
            std::move(target),
            compoundSymbol(op.type),
            std::move(right)
        );

        requireEndOfStatement();

        return std::make_unique<ExpressionStmt>(
            start.line,
            std::make_unique<AssignExpr>(
                op.line,
                std::move(targetCopy),
                std::move(combined)
            )
        );
    }

    requireEndOfStatement();

    return std::make_unique<ExpressionStmt>(
        start.line,
        std::move(value)
    );
}

std::unique_ptr<Expr> Parser::expression() {
    return assignment();
}

std::unique_ptr<Expr> Parser::condition() {
    if (match(TokenType::LeftParen)) {
        auto expr = expression();

        consume(
            TokenType::RightParen,
            "expected ')' after the condition"
        );

        return expr;
    }

    return expression();
}

std::unique_ptr<Expr> Parser::assignment() {
    auto expr = orExpr();

    if (match(TokenType::Equal)) {
        const Token equals = previous();

        auto value = assignment();

        bool valid =
            dynamic_cast<IdentifierExpr*>(expr.get()) != nullptr ||
            dynamic_cast<IndexExpr*>(expr.get()) != nullptr ||
            dynamic_cast<PropertyExpr*>(expr.get()) != nullptr;

        if (!valid) {
            fail(
                equals,
                "this expression cannot be assigned to"
            );
        }

        return std::make_unique<AssignExpr>(
            equals.line,
            std::move(expr),
            std::move(value)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::orExpr() {
    auto expr = andExpr();

    while (match(TokenType::Or)) {
        const Token op = previous();
        auto right = andExpr();

        expr = std::make_unique<LogicalExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::andExpr() {
    auto expr = equality();

    while (match(TokenType::And)) {
        const Token op = previous();
        auto right = equality();

        expr = std::make_unique<LogicalExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();

    while (matchEither(TokenType::EqualEqual,
                       TokenType::BangEqual)) {
        const Token op = previous();
        auto right = comparison();

        expr = std::make_unique<BinaryExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();

    while (matchEither(TokenType::Greater, TokenType::GreaterEqual) ||
           matchEither(TokenType::Less, TokenType::LessEqual)) {
        const Token op = previous();
        auto right = term();

        expr = std::make_unique<BinaryExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();

    while (matchEither(TokenType::Plus, TokenType::Minus)) {
        const Token op = previous();
        auto right = factor();

        expr = std::make_unique<BinaryExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();

    while (matchEither(TokenType::Star, TokenType::Slash) ||
           match(TokenType::Percent)) {
        const Token op = previous();
        auto right = unary();

        expr = std::make_unique<BinaryExpr>(
            op.line,
            std::move(expr),
            op.text,
            std::move(right)
        );
    }

    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (matchEither(TokenType::Bang, TokenType::Not) ||
        match(TokenType::Minus)) {
        const Token op = previous();
        auto right = unary();

        if (op.type == TokenType::Not) {
            return std::make_unique<UnaryExpr>(
                op.line,
                "!",
                std::move(right)
            );
        }

        return std::make_unique<UnaryExpr>(
            op.line,
            op.text,
            std::move(right)
        );
    }

    return call();
}

std::unique_ptr<Expr> Parser::call() {
    auto expr = primary();

    while (true) {
        if (match(TokenType::LeftParen)) {
            const Token open = previous();

            std::vector<std::unique_ptr<Expr>> arguments;

            if (!check(TokenType::RightParen)) {
                do {
                    arguments.push_back(expression());
                } while (match(TokenType::Comma));
            }

            consume(
                TokenType::RightParen,
                "expected ')' after the argument list"
            );

            expr = std::make_unique<CallExpr>(
                open.line,
                std::move(expr),
                std::move(arguments)
            );

            continue;
        }

        if (match(TokenType::LeftBracket)) {
            const Token open = previous();

            auto index = expression();

            consume(
                TokenType::RightBracket,
                "expected ']' after the index"
            );

            expr = std::make_unique<IndexExpr>(
                open.line,
                std::move(expr),
                std::move(index)
            );

            continue;
        }

        if (match(TokenType::Dot)) {
            const Token name = consume(
                TokenType::Identifier,
                "expected a field or method name after '.'"
            );

            expr = std::make_unique<PropertyExpr>(
                name.line,
                std::move(expr),
                name.text
            );

            continue;
        }

        break;
    }

    return expr;
}

std::unique_ptr<Expr> Parser::superExpression() {
    const Token keyword = previous();

    std::unique_ptr<Expr> callee;

    if (match(TokenType::Dot)) {
        const Token name = consume(
            TokenType::Identifier,
            "expected a method name after 'super.'"
        );

        callee = std::make_unique<IdentifierExpr>(
            keyword.line,
            name.text
        );
    } else {
        callee = std::make_unique<IdentifierExpr>(
            keyword.line,
            "constructor"
        );
    }

    std::vector<std::unique_ptr<Expr>> arguments;

    if (match(TokenType::LeftParen)) {
        if (!check(TokenType::RightParen)) {
            do {
                arguments.push_back(expression());
            } while (match(TokenType::Comma));
        }

        consume(
            TokenType::RightParen,
            "expected ')' after the argument list"
        );
    }

    return std::make_unique<SuperExpr>(
        keyword.line,
        std::move(callee),
        std::move(arguments)
    );
}

std::unique_ptr<Expr> Parser::newExpression() {
    const Token keyword = previous();

    std::unique_ptr<Expr> callee = primary();

    while (match(TokenType::Dot)) {
        const Token name = consume(
            TokenType::Identifier,
            "expected a class name after '.'"
        );

        callee = std::make_unique<PropertyExpr>(
            name.line,
            std::move(callee),
            name.text
        );
    }

    std::vector<std::unique_ptr<Expr>> arguments;

    if (match(TokenType::LeftParen)) {
        if (!check(TokenType::RightParen)) {
            do {
                arguments.push_back(expression());
            } while (match(TokenType::Comma));
        }

        consume(
            TokenType::RightParen,
            "expected ')' after the argument list"
        );
    }

    return std::make_unique<NewExpr>(
        keyword.line,
        std::move(callee),
        std::move(arguments)
    );
}

std::unique_ptr<Expr> Parser::primary() {
    if (match(TokenType::Number)) {
        const Token token = previous();

        return std::make_unique<NumberExpr>(
            token.line,
            token.number
        );
    }

    if (match(TokenType::String)) {
        const Token token = previous();

        if (token.text.find(INTERPOLATE_OPEN) != std::string::npos ||
            token.text.find(INTERPOLATE_CLOSE) != std::string::npos) {
            return interpolated(token);
        }

        return std::make_unique<StringExpr>(
            token.line,
            token.text
        );
    }

    if (match(TokenType::True)) {
        return std::make_unique<BooleanExpr>(
            previous().line,
            true
        );
    }

    if (match(TokenType::False)) {
        return std::make_unique<BooleanExpr>(
            previous().line,
            false
        );
    }

    if (match(TokenType::Nil)) {
        return std::make_unique<NilExpr>(previous().line);
    }

    if (match(TokenType::Super)) {
        return superExpression();
    }

    if (match(TokenType::Identifier)) {
        const Token token = previous();

        return std::make_unique<IdentifierExpr>(
            token.line,
            token.text
        );
    }

    if (match(TokenType::LeftParen)) {
        auto expr = expression();

        consume(
            TokenType::RightParen,
            "expected ')' to close the expression"
        );

        return expr;
    }

    if (match(TokenType::This)) {
        return std::make_unique<ThisExpr>(previous().line);
    }

    if (match(TokenType::Function)) {
        return functionExpression();
    }

    if (match(TokenType::New)) {
        return newExpression();
    }

    if (match(TokenType::LeftBracket)) {
        return listLiteral();
    }

    if (match(TokenType::LeftBrace)) {
        return mapLiteral();
    }

    fail(
        peek(),
        std::string("expected an expression, found '") +
        tokenTypeName(peek().type) +
        "'"
    );
}

std::unique_ptr<Expr> Parser::interpolated(const Token& token) {
    std::unique_ptr<Expr> result;
    std::string literal;

    size_t i = 0;

    while (i < token.text.size()) {
        char c = token.text[i];

        if (c != INTERPOLATE_OPEN) {
            if (c == INTERPOLATE_CLOSE) {
                fail(
                    token,
                    "this string has a '}' with nothing to fill"
                );
            }

            literal += c;
            i++;
            continue;
        }

        size_t end = i + 1;
        int depth = 0;

        while (end < token.text.size()) {
            char inner = token.text[end];

            if (inner == INTERPOLATE_OPEN) {
                depth++;
            } else if (inner == INTERPOLATE_CLOSE) {
                if (depth == 0) {
                    break;
                }

                depth--;
            }

            end++;
        }

        if (end >= token.text.size()) {
            fail(
                token,
                "this string has a '{' that was never closed"
            );
        }

        std::string source = token.text.substr(i + 1, end - i - 1);

        if (source.empty()) {
            fail(token, "this string has an empty '{}' to fill");
        }

        if (!literal.empty()) {
            auto text = std::make_unique<StringExpr>(token.line, literal);
            literal.clear();

            if (result) {
                result = std::make_unique<BinaryExpr>(
                    token.line,
                    std::move(result),
                    "+",
                    std::move(text)
                );
            } else {
                result = std::move(text);
            }
        }

        Parser inner(Lexer(source).tokenize());

        std::unique_ptr<Expr> value;

        try {
            value = inner.expression();
        } catch (const NovaError& problem) {
            fail(
                token,
                "cannot read the expression inside '{}': " +
                    std::string(problem.what())
            );
        }

        if (!inner.usedEveryToken()) {
            fail(
                token,
                "cannot read the expression inside '{}': extra text"
            );
        }

        if (result) {
            result = std::make_unique<BinaryExpr>(
                token.line,
                std::move(result),
                "+",
                std::move(value)
            );
        } else {
            result = std::move(value);
        }

        i = end + 1;
    }

    if (!literal.empty()) {
        auto text = std::make_unique<StringExpr>(token.line, literal);

        if (result) {
            result = std::make_unique<BinaryExpr>(
                token.line,
                std::move(result),
                "+",
                std::move(text)
            );
        } else {
            result = std::move(text);
        }
    }

    if (!result) {
        return std::make_unique<StringExpr>(token.line, "");
    }

    std::vector<std::unique_ptr<Expr>> arguments;
    arguments.push_back(std::move(result));

    auto convert = std::make_unique<IdentifierExpr>(
        token.line,
        "str"
    );

    return std::make_unique<CallExpr>(
        token.line,
        std::move(convert),
        std::move(arguments)
    );
}

std::unique_ptr<Expr> Parser::functionExpression() {
    const Token keyword = previous();

    std::vector<std::string> parameters = parameterList();

    consume(
        TokenType::LeftBrace,
        "expected '{' before the function body"
    );

    auto body = blockStatement();

    return std::make_unique<FunctionExpr>(
        keyword.line,
        std::move(parameters),
        std::move(body)
    );
}

std::unique_ptr<Expr> Parser::listLiteral() {
    const Token open = previous();

    std::vector<std::unique_ptr<Expr>> items;

    if (!check(TokenType::RightBracket)) {
        do {
            if (check(TokenType::RightBracket)) {
                break;
            }

            items.push_back(expression());
        } while (match(TokenType::Comma));
    }

    consume(
        TokenType::RightBracket,
        "expected ']' to close the list"
    );

    return std::make_unique<ListExpr>(
        open.line,
        std::move(items)
    );
}

std::unique_ptr<Expr> Parser::mapLiteral() {
    const Token open = previous();

    std::vector<
        std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>
    > entries;

    if (!check(TokenType::RightBrace)) {
        do {
            if (check(TokenType::RightBrace)) {
                break;
            }

            std::unique_ptr<Expr> key;

            if (check(TokenType::Identifier) &&
                tokenAt(1).type == TokenType::Colon) {
                const Token name = advance();

                key = std::make_unique<StringExpr>(
                    name.line,
                    name.text
                );
            } else {
                key = expression();
            }

            consume(
                TokenType::Colon,
                "expected ':' after a map key"
            );

            auto value = expression();

            entries.emplace_back(
                std::move(key),
                std::move(value)
            );
        } while (match(TokenType::Comma));
    }

    consume(
        TokenType::RightBrace,
        "expected '}' to close the map"
    );

    return std::make_unique<MapExpr>(
        open.line,
        std::move(entries)
    );
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool Parser::startsForIn() const {
    return check(TokenType::LeftParen) &&
        tokenAt(1).type == TokenType::Let &&
        tokenAt(2).type == TokenType::Identifier &&
        tokenAt(3).type == TokenType::In;
}

bool Parser::matchEither(
    TokenType first,
    TokenType second
) {
    return match(first) || match(second);
}

bool Parser::matchCompoundOperator() {
    static const TokenType compoundTypes[] = {
        TokenType::PlusEqual,
        TokenType::MinusEqual,
        TokenType::StarEqual,
        TokenType::SlashEqual,
        TokenType::PercentEqual
    };

    for (TokenType type : compoundTypes) {
        if (match(type)) {
            return true;
        }
    }

    return false;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) {
        return type == TokenType::EndOfFile;
    }

    return peek().type == type;
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::EndOfFile;
}

void Parser::skipToken(TokenType type) {
    if (check(type)) {
        advance();
    }
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        current++;
    }

    return previous();
}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::tokenAt(size_t offset) const {
    if (current + offset < tokens.size()) {
        return tokens[current + offset];
    }

    return tokens.back();
}

const Token& Parser::peekNext() const {
    if (current + 1 < tokens.size()) {
        return tokens[current + 1];
    }

    return tokens.back();
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::consume(
    TokenType type,
    const std::string& message
) {
    if (check(type)) {
        return advance();
    }

    fail(peek(), message);
}

void Parser::requireEndOfStatement() {
    if (match(TokenType::Semicolon)) {
        return;
    }

    fail(
        peek(),
        std::string("expected ';' at the end of the statement, found '") +
        tokenTypeName(peek().type) +
        "'"
    );
}

void Parser::fail(
    const Token& token,
    const std::string& message
) const {
    throw ParserError(message, {
        token.line,
        token.column
    });
}
