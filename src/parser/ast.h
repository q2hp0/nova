#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct BlockStmt;

struct Expr {
    explicit Expr(int line)
        : line(line) {
    }

    virtual ~Expr() = default;

    int line;
};

struct NumberExpr : Expr {
    double value;

    NumberExpr(int line, double value)
        : Expr(line), value(value) {
    }
};

struct StringExpr : Expr {
    std::string value;

    StringExpr(int line, const std::string& value)
        : Expr(line), value(value) {
    }
};

struct BooleanExpr : Expr {
    bool value;

    BooleanExpr(int line, bool value)
        : Expr(line), value(value) {
    }
};

struct NilExpr : Expr {
    NilExpr(int line)
        : Expr(line) {
    }
};

struct IdentifierExpr : Expr {
    std::string name;

    IdentifierExpr(int line, const std::string& name)
        : Expr(line), name(name) {
    }
};

struct ListExpr : Expr {
    std::vector<std::unique_ptr<Expr>> items;

    ListExpr(
        int line,
        std::vector<std::unique_ptr<Expr>> items
    )
        : Expr(line), items(std::move(items)) {
    }
};

struct MapExpr : Expr {
    std::vector<
        std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>
    > entries;

    MapExpr(
        int line,
        std::vector<
            std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>
        > entries
    )
        : Expr(line), entries(std::move(entries)) {
    }
};

struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> right;

    UnaryExpr(
        int line,
        const std::string& op,
        std::unique_ptr<Expr> right
    )
        : Expr(line), op(op), right(std::move(right)) {
    }
};

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    std::string op;
    std::unique_ptr<Expr> right;

    BinaryExpr(
        int line,
        std::unique_ptr<Expr> left,
        const std::string& op,
        std::unique_ptr<Expr> right
    )
        : Expr(line),
          left(std::move(left)),
          op(op),
          right(std::move(right)) {
    }
};

struct LogicalExpr : Expr {
    std::unique_ptr<Expr> left;
    std::string op;
    std::unique_ptr<Expr> right;

    LogicalExpr(
        int line,
        std::unique_ptr<Expr> left,
        const std::string& op,
        std::unique_ptr<Expr> right
    )
        : Expr(line),
          left(std::move(left)),
          op(op),
          right(std::move(right)) {
    }
};

struct AssignExpr : Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;

    AssignExpr(
        int line,
        std::unique_ptr<Expr> target,
        std::unique_ptr<Expr> value
    )
        : Expr(line),
          target(std::move(target)),
          value(std::move(value)) {
    }
};

struct IndexExpr : Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> index;

    IndexExpr(
        int line,
        std::unique_ptr<Expr> target,
        std::unique_ptr<Expr> index
    )
        : Expr(line),
          target(std::move(target)),
          index(std::move(index)) {
    }
};

struct PropertyExpr : Expr {
    std::unique_ptr<Expr> target;
    std::string name;

    PropertyExpr(
        int line,
        std::unique_ptr<Expr> target,
        const std::string& name
    )
        : Expr(line),
          target(std::move(target)),
          name(name) {
    }
};

struct FunctionExpr : Expr {
    std::vector<std::string> parameters;
    std::unique_ptr<BlockStmt> body;

    FunctionExpr(
        int line,
        std::vector<std::string> parameters,
        std::unique_ptr<BlockStmt> body
    )
        : Expr(line),
          parameters(std::move(parameters)),
          body(std::move(body)) {
    }

    ~FunctionExpr();
};

struct ThisExpr : Expr {
    explicit ThisExpr(int line)
        : Expr(line) {
    }
};

struct SuperExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    SuperExpr(
        int line,
        std::unique_ptr<Expr> callee,
        std::vector<std::unique_ptr<Expr>> arguments
    )
        : Expr(line),
          callee(std::move(callee)),
          arguments(std::move(arguments)) {
    }
};

struct NewExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    NewExpr(
        int line,
        std::unique_ptr<Expr> callee,
        std::vector<std::unique_ptr<Expr>> arguments
    )
        : Expr(line),
          callee(std::move(callee)),
          arguments(std::move(arguments)) {
    }
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(
        int line,
        std::unique_ptr<Expr> callee,
        std::vector<std::unique_ptr<Expr>> arguments
    )
        : Expr(line),
          callee(std::move(callee)),
          arguments(std::move(arguments)) {
    }
};

struct Stmt {
    explicit Stmt(int line)
        : line(line) {
    }

    virtual ~Stmt() = default;

    int line;
};

struct ExpressionStmt : Stmt {
    std::unique_ptr<Expr> expression;

    ExpressionStmt(int line, std::unique_ptr<Expr> expression)
        : Stmt(line), expression(std::move(expression)) {
    }
};

enum class PatternKind {
    Name,
    List,
    Map
};

struct Pattern {
    PatternKind kind = PatternKind::Name;
    std::string name;
    std::string key;
    std::vector<std::unique_ptr<Pattern>> parts;
    int position = -1;
    int rest = -1;
};

struct LetStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> initializer;

    LetStmt(
        int line,
        const std::string& name,
        std::unique_ptr<Expr> initializer
    )
        : Stmt(line),
          name(name),
          initializer(std::move(initializer)) {
    }
};

struct DestructureStmt : Stmt {
    std::unique_ptr<Pattern> pattern;
    std::unique_ptr<Expr> initializer;

    DestructureStmt(
        int line,
        std::unique_ptr<Pattern> pattern,
        std::unique_ptr<Expr> initializer
    )
        : Stmt(line),
          pattern(std::move(pattern)),
          initializer(std::move(initializer)) {
    }
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    BlockStmt(int line)
        : Stmt(line) {
    }
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;

    IfStmt(
        int line,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Stmt> thenBranch,
        std::unique_ptr<Stmt> elseBranch
    )
        : Stmt(line),
          condition(std::move(condition)),
          thenBranch(std::move(thenBranch)),
          elseBranch(std::move(elseBranch)) {
    }
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(
        int line,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Stmt> body
    )
        : Stmt(line),
          condition(std::move(condition)),
          body(std::move(body)) {
    }
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt> initializer;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> increment;
    std::unique_ptr<Stmt> body;

    ForStmt(
        int line,
        std::unique_ptr<Stmt> initializer,
        std::unique_ptr<Expr> condition,
        std::unique_ptr<Expr> increment,
        std::unique_ptr<Stmt> body
    )
        : Stmt(line),
          initializer(std::move(initializer)),
          condition(std::move(condition)),
          increment(std::move(increment)),
          body(std::move(body)) {
    }
};

struct ForInStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> sequence;
    std::unique_ptr<Stmt> body;

    ForInStmt(
        int line,
        const std::string& name,
        std::unique_ptr<Expr> sequence,
        std::unique_ptr<Stmt> body
    )
        : Stmt(line),
          name(name),
          sequence(std::move(sequence)),
          body(std::move(body)) {
    }
};

struct SwitchCase {
    std::unique_ptr<Expr> value;
    std::unique_ptr<Stmt> body;
};

struct SwitchStmt : Stmt {
    std::unique_ptr<Expr> subject;
    std::vector<SwitchCase> cases;

    SwitchStmt(
        int line,
        std::unique_ptr<Expr> subject,
        std::vector<SwitchCase> cases
    )
        : Stmt(line),
          subject(std::move(subject)),
          cases(std::move(cases)) {
    }
};

struct BreakStmt : Stmt {
    BreakStmt(int line)
        : Stmt(line) {
    }
};

struct ContinueStmt : Stmt {
    ContinueStmt(int line)
        : Stmt(line) {
    }
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;

    ReturnStmt(int line, std::unique_ptr<Expr> value)
        : Stmt(line), value(std::move(value)) {
    }
};

struct ClassMethod {
    std::string name;
    std::vector<std::string> parameters;
    std::unique_ptr<BlockStmt> body;
    bool isStatic = false;
};

struct ClassStmt : Stmt {
    std::string name;
    std::string baseName;
    std::vector<std::string> fields;
    std::vector<std::unique_ptr<Expr>> fieldDefaults;
    std::vector<ClassMethod> methods;

    ClassStmt(
        int line,
        const std::string& name,
        const std::string& baseName
    )
        : Stmt(line), name(name), baseName(baseName) {
    }
};

struct FunctionStmt : Stmt {
    std::string name;
    std::vector<std::string> parameters;
    std::unique_ptr<BlockStmt> body;

    FunctionStmt(
        int line,
        const std::string& name,
        std::vector<std::string> parameters,
        std::unique_ptr<BlockStmt> body
    )
        : Stmt(line),
          name(name),
          parameters(std::move(parameters)),
          body(std::move(body)) {
    }
};

struct ThrowStmt : Stmt {
    std::unique_ptr<Expr> value;

    ThrowStmt(int line, std::unique_ptr<Expr> value)
        : Stmt(line), value(std::move(value)) {
    }
};

struct TryStmt : Stmt {
    std::unique_ptr<BlockStmt> body;
    std::string caught;
    std::unique_ptr<BlockStmt> handler;
    std::unique_ptr<BlockStmt> cleanup;

    TryStmt(int line, std::unique_ptr<BlockStmt> body)
        : Stmt(line), body(std::move(body)) {
    }
};

struct ImportStmt : Stmt {
    std::string path;
    std::string alias;

    ImportStmt(int line, const std::string& path)
        : Stmt(line), path(path) {
    }
};

struct ExportStmt : Stmt {
    std::vector<std::string> names;

    ExportStmt(int line, std::vector<std::string> names)
        : Stmt(line), names(std::move(names)) {
    }
};

std::unique_ptr<Stmt> cloneStmt(const Stmt* statement);
std::unique_ptr<Expr> cloneExpr(const Expr* expression);
