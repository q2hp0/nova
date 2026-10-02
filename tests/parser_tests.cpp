#include "errors/errors.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void expectTrue(
    const std::string& name,
    bool condition,
    const std::string& detail
) {
    checks++;

    if (condition) {
        return;
    }

    failures++;
    std::cout << "FAILED " << name << ": " << detail << "\n";
}

std::vector<std::unique_ptr<Stmt>> parse(const std::string& source) {
    Parser parser(Lexer(source).tokenize());

    return parser.parse();
}

std::unique_ptr<Stmt> parseOne(const std::string& source) {
    auto statements = parse(source);

    if (statements.size() != 1) {
        throw std::runtime_error(
            "expected one statement, found " +
            std::to_string(statements.size())
        );
    }

    return std::move(statements.front());
}

void expectError(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    try {
        parse(source);
    } catch (const NovaError& problem) {
        std::string detail = problem.what();

        if (detail.find(expected) == std::string::npos) {
            failures++;
            std::cout << "FAILED " << name << ": message was '"
                      << detail << "'\n";
        }

        return;
    } catch (const std::exception& problem) {
        failures++;
        std::cout << "FAILED " << name << ": wrong error: "
                  << problem.what() << "\n";
        return;
    }

    failures++;
    std::cout << "FAILED " << name << ": no error was raised\n";
}

const char* statementName(const Stmt& node) {
    if (dynamic_cast<const LetStmt*>(&node) != nullptr) {
        return "let";
    }

    if (dynamic_cast<const BlockStmt*>(&node) != nullptr) {
        return "block";
    }

    if (dynamic_cast<const IfStmt*>(&node) != nullptr) {
        return "if";
    }

    if (dynamic_cast<const WhileStmt*>(&node) != nullptr) {
        return "while";
    }

    if (dynamic_cast<const ForStmt*>(&node) != nullptr) {
        return "for";
    }

    if (dynamic_cast<const ForInStmt*>(&node) != nullptr) {
        return "for in";
    }

    if (dynamic_cast<const BreakStmt*>(&node) != nullptr) {
        return "break";
    }

    if (dynamic_cast<const ContinueStmt*>(&node) != nullptr) {
        return "continue";
    }

    if (dynamic_cast<const ReturnStmt*>(&node) != nullptr) {
        return "return";
    }

    if (dynamic_cast<const DestructureStmt*>(&node) != nullptr) {
        return "destructure";
    }

    if (dynamic_cast<const SwitchStmt*>(&node) != nullptr) {
        return "switch";
    }

    if (dynamic_cast<const FunctionStmt*>(&node) != nullptr) {
        return "function";
    }

    if (dynamic_cast<const ClassStmt*>(&node) != nullptr) {
        return "class";
    }

    if (dynamic_cast<const ThrowStmt*>(&node) != nullptr) {
        return "throw";
    }

    if (dynamic_cast<const TryStmt*>(&node) != nullptr) {
        return "try";
    }

    if (dynamic_cast<const ImportStmt*>(&node) != nullptr) {
        return "import";
    }

    if (dynamic_cast<const ExportStmt*>(&node) != nullptr) {
        return "export";
    }

    if (dynamic_cast<const ExpressionStmt*>(&node) != nullptr) {
        return "expression";
    }

    return "unknown";
}

const char* expressionName(const Expr& node) {
    if (dynamic_cast<const NumberExpr*>(&node) != nullptr) {
        return "number";
    }

    if (dynamic_cast<const StringExpr*>(&node) != nullptr) {
        return "string";
    }

    if (dynamic_cast<const BooleanExpr*>(&node) != nullptr) {
        return "boolean";
    }

    if (dynamic_cast<const NilExpr*>(&node) != nullptr) {
        return "nil";
    }

    if (dynamic_cast<const IdentifierExpr*>(&node) != nullptr) {
        return "name";
    }

    if (dynamic_cast<const ListExpr*>(&node) != nullptr) {
        return "list";
    }

    if (dynamic_cast<const MapExpr*>(&node) != nullptr) {
        return "map";
    }

    if (dynamic_cast<const UnaryExpr*>(&node) != nullptr) {
        return "unary";
    }

    if (dynamic_cast<const BinaryExpr*>(&node) != nullptr) {
        return "binary";
    }

    if (dynamic_cast<const LogicalExpr*>(&node) != nullptr) {
        return "logical";
    }

    if (dynamic_cast<const AssignExpr*>(&node) != nullptr) {
        return "assign";
    }

    if (dynamic_cast<const IndexExpr*>(&node) != nullptr) {
        return "index";
    }

    if (dynamic_cast<const PropertyExpr*>(&node) != nullptr) {
        return "property";
    }

    if (dynamic_cast<const FunctionExpr*>(&node) != nullptr) {
        return "function";
    }

    if (dynamic_cast<const CallExpr*>(&node) != nullptr) {
        return "call";
    }

    if (dynamic_cast<const ThisExpr*>(&node) != nullptr) {
        return "this";
    }

    if (dynamic_cast<const SuperExpr*>(&node) != nullptr) {
        return "super";
    }

    if (dynamic_cast<const NewExpr*>(&node) != nullptr) {
        return "new";
    }

    return "unknown";
}

const Expr& expressionOf(const Stmt& node) {
    const auto* wrapper = dynamic_cast<const ExpressionStmt*>(&node);

    if (wrapper == nullptr) {
        throw std::runtime_error("the statement holds no expression");
    }

    return *wrapper->expression;
}

void expectStatement(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    std::string actual;

    try {
        std::unique_ptr<Stmt> node = parseOne(source);
        actual = statementName(*node);
    } catch (const std::exception& problem) {
        failures++;
        std::cout << "FAILED " << name << ": " << problem.what() << "\n";
        return;
    }

    if (actual != expected) {
        failures++;
        std::cout << "FAILED " << name << ": expected " << expected
                  << " found " << actual << "\n";
    }
}

void expectExpression(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    std::string actual;

    try {
        std::unique_ptr<Stmt> node = parseOne(source);
        actual = expressionName(expressionOf(*node));
    } catch (const std::exception& problem) {
        failures++;
        std::cout << "FAILED " << name << ": " << problem.what() << "\n";
        return;
    }

    if (actual != expected) {
        failures++;
        std::cout << "FAILED " << name << ": expected " << expected
                  << " found " << actual << "\n";
    }
}

void destructureTests() {
    {
        auto statements = parse("let [a, b] = xs;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a list pattern builds a destructure",
            node != nullptr,
            statementName(*statements[0])
        );

        if (node != nullptr) {
            expectTrue(
                "a list pattern holds its parts",
                node->pattern->kind == PatternKind::List &&
                    node->pattern->parts.size() == 2 &&
                    node->pattern->parts[0]->name == "a" &&
                    node->pattern->parts[1]->name == "b",
                "shape"
            );

            expectTrue(
                "a plain list pattern has no rest",
                node->pattern->rest == -1,
                std::to_string(node->pattern->rest)
            );
        }
    }

    {
        auto statements = parse("let {a, b: c} = m;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a map pattern builds a destructure",
            node != nullptr && node->pattern->kind == PatternKind::Map,
            "shape"
        );

        if (node != nullptr) {
            expectTrue(
                "a renamed key keeps its name",
                node->pattern->parts[1]->key == "b" &&
                    node->pattern->parts[1]->name == "c",
                node->pattern->parts[1]->key
            );
        }
    }

    {
        auto statements = parse("let [a, ...rest] = xs;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a rest pattern records its position",
            node != nullptr && node->pattern->rest == 1,
            "shape"
        );
    }

    {
        auto statements = parse("let {a: {b}} = m;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a nested pattern nests",
            node != nullptr &&
                node->pattern->parts[0]->kind == PatternKind::Map &&
                node->pattern->parts[0]->parts[0]->name == "b",
            "shape"
        );
    }

    {
        auto statements = parse("let {\"a key\": v} = m;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a string key works in a pattern",
            node != nullptr &&
                node->pattern->parts[0]->key == "a key",
            "shape"
        );
    }

    {
        bool failed = false;

        try {
            parse("let [a, a] = xs;");
        } catch (const NovaError& problem) {
            failed = std::string(problem.what()).find(
                "already in the pattern"
            ) != std::string::npos;
        }

        expectTrue("a repeated name fails", failed, "no error");
    }

    {
        auto statements = parse("let [, a, , b] = xs;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "a skipped slot keeps its position",
            node != nullptr &&
                node->pattern->parts[0]->position == 1 &&
                node->pattern->parts[1]->position == 3,
            "shape"
        );
    }

    {
        auto statements = parse("let {a: _, b} = m;");
        auto node = dynamic_cast<DestructureStmt*>(statements[0].get());
        expectTrue(
            "an underscore holds no name",
            node != nullptr &&
                node->pattern->parts[0]->name == "_" &&
                node->pattern->parts[1]->name == "b",
            "shape"
        );
    }

    expectError(
        "a pattern without a value",
        "let [a, b];",
        "a pattern needs a value"
    );
    expectError("an empty list pattern", "let [] = xs;", "holds no names");
    expectError("an empty map pattern", "let {} = m;", "holds no names");
    expectError("a rest with no names", "let [...r] = xs;", "before '...'");
    expectError("a rest without a name", "let [a, ...] = xs;", "after '...'");
    expectError(
        "an unclosed list pattern",
        "let [a, b = xs;",
        "expected ']'"
    );
    expectError(
        "an unclosed map pattern",
        "let {a = m;",
        "expected '}'"
    );
    expectError("a bare key in a pattern", "let {a: } = m;", "inside the pattern");
}

void shapeTests() {
    expectStatement("a let statement", "let x = 1;", "let");
    expectStatement("a class", "class P { }", "class");
    expectStatement(
        "a class with a constructor",
        "class P { constructor() { } }",
        "class"
    );
    expectStatement(
        "a class with fields",
        "class P { x = 1, y = 2 }",
        "class"
    );
    expectStatement(
        "a class with a static method",
        "class P { static make() { return 1; } }",
        "class"
    );
    expectStatement(
        "a subclass",
        "class P extends Q { }",
        "class"
    );
    expectStatement(
        "a try statement",
        "try { print(1); } catch e { }",
        "try"
    );
    expectStatement(
        "a try with no binding",
        "try { } catch { }",
        "try"
    );
    expectStatement(
        "a try with finally",
        "try { } finally { }",
        "try"
    );
    expectStatement("a throw", "throw 1;", "throw");
    expectStatement("an import", "import \"a.nova\";", "import");
    expectStatement("an export", "export a, b;", "export");

    {
        auto node = parseOne("import \"a.nova\" as m;");
        auto import = dynamic_cast<ImportStmt*>(node.get());

        expectTrue(
            "an import keeps its path",
            import != nullptr && import->path == "a.nova",
            "path was not 'a.nova'"
        );
        expectTrue(
            "an import keeps its alias",
            import != nullptr && import->alias == "m",
            "alias was not 'm'"
        );
    }

    {
        auto node = parseOne("import \"a.nova\";");
        auto import = dynamic_cast<ImportStmt*>(node.get());

        expectTrue(
            "an import can have no alias",
            import != nullptr && import->alias.empty(),
            "alias was not empty"
        );
    }

    {
        auto node = parseOne("export { a, b };");
        auto exportNode = dynamic_cast<ExportStmt*>(node.get());

        expectTrue(
            "an export list keeps its names",
            exportNode != nullptr &&
                exportNode->names == std::vector<std::string>({"a", "b"}),
            "names did not match"
        );
    }
    expectStatement("a plain block", "{ print(1); }", "block");
    expectStatement("an if statement", "if x { }", "if");
    expectStatement(
        "an if with an else",
        "if x { } else { }",
        "if"
    );
    expectStatement("an else if", "if x { } else if y { }", "if");
    expectStatement("a while statement", "while x { }", "while");
    expectStatement("a for statement", "for (let i = 0; i < 1; i = i + 1) { }", "for");
    expectStatement("a for in statement", "for (let v in [1]) { }", "for in");
    expectStatement("a break", "while true { break; }", "while");
    expectStatement("a function", "function f() { return 1; }", "function");
    expectStatement("a return", "function f() { return; }", "function");
    expectStatement("a switch", "switch 1 { case 1: }", "switch");
    expectStatement(
        "a switch with a default",
        "switch 1 { case 1: default: }",
        "switch"
    );
    expectStatement(
        "a switch with a default first",
        "switch 1 { default: case 1: }",
        "switch"
    );
    expectStatement("a switch with only a default", "switch 1 { default: }", "switch");
    expectStatement(
        "a switch with an empty case",
        "switch 1 { case 1: case 2: print(1); }",
        "switch"
    );
    expectStatement(
        "a switch case with a block",
        "switch 1 { case 1: { print(1); } }",
        "switch"
    );
}

void expressionTests() {
    expectExpression("a new expression", "new P();", "new");
    expectExpression("a new without parens", "new P;", "new");
    expectExpression("a new with arguments", "new P(1, 2);", "new");
    expectExpression("this", "this;", "this");
    expectExpression("super", "super();", "super");
    expectExpression("super on a method", "super.m();", "super");
    expectExpression("a number", "1;", "number");
    expectExpression("a string", "\"a\";", "string");
    expectExpression("a hole that is the whole string", "\"{x}\";", "call");
    expectExpression("text around a hole", "\"a{x}\";", "call");
    expectExpression("a hole with text after it", "\"{x}b\";", "call");
    expectExpression("two holes", "\"{x}{y}\";", "call");
    expectExpression("a hole with an operation", "\"{x + 1}\";", "call");
    expectExpression(
        "an escaped brace is plain text",
        "\"\\{x\\}\";",
        "string"
    );
    expectExpression("true", "true;", "boolean");
    expectExpression("nil", "nil;", "nil");
    expectExpression("null", "null;", "nil");
    expectExpression("a name", "x;", "name");
    expectExpression("a list", "[1, 2];", "list");
    expectExpression("a map", "({\"a\": 1});", "map");
    expectExpression("a map with bare keys", "({a: 1});", "map");
    expectStatement("a block that starts like a map", "{ 1; }", "block");
    expectExpression("a negative number", "-1;", "unary");
    expectExpression("a not", "not true;", "unary");
    expectExpression("an addition", "1 + 2;", "binary");
    expectExpression("an and", "true and false;", "logical");
    expectExpression("an or", "true or false;", "logical");
    expectExpression("an assignment", "x = 1;", "assign");
    expectExpression("a compound assignment", "x += 1;", "assign");
    expectExpression("an index", "x[0];", "index");
    expectExpression("a property", "x.y;", "property");
    expectExpression("a call", "f();", "call");
    expectExpression("a method call", "x.y(1);", "call");
    expectExpression("an anonymous function", "(function() { return 1; });", "function");
    expectExpression("a call that is then read", "x.y(1).z;", "property");
    expectExpression("a call of a call", "x.y(1).z(2);", "call");
    expectStatement("an if without braces", "if x print(1);", "if");
    expectStatement("a while without braces", "while x print(1);", "while");
    expectExpression("a chained index", "x[0][1];", "index");
}

void detailTests() {
    auto let = std::move(parseOne("let answer = 42;"));
    const auto* declaration = dynamic_cast<LetStmt*>(let.get());

    expectTrue(
        "a let keeps its name",
        declaration != nullptr && declaration->name == "answer",
        "the name was not kept"
    );

    expectTrue(
        "a let keeps its line",
        declaration != nullptr && declaration->line == 1,
        "the line was not kept"
    );

    auto chain = std::move(parseOne("x.y(1);"));
    const auto& call = expressionOf(*chain);
    const auto* invocation = dynamic_cast<const CallExpr*>(&call);

    const auto* method = invocation == nullptr ?
        nullptr : dynamic_cast<const PropertyExpr*>(invocation->callee.get());

    expectTrue(
        "a method call keeps its name",
        method != nullptr && method->name == "y",
        "the method name was not kept"
    );

    expectTrue(
        "a call keeps its arguments",
        invocation != nullptr && invocation->arguments.size() == 1,
        "the argument count was not kept"
    );

    auto sum = std::move(parseOne("1 + 2 * 3;"));
    const auto* addition = dynamic_cast<const BinaryExpr*>(&expressionOf(*sum));

    expectTrue(
        "multiplication binds tighter than addition",
        addition != nullptr &&
            expressionName(*addition->right) == "binary",
        "the tree has the wrong shape"
    );

    const auto* multiplication = dynamic_cast<const BinaryExpr*>(
        addition == nullptr ? nullptr : addition->right.get()
    );

    expectTrue(
        "the inner operation is a multiplication",
        multiplication != nullptr && multiplication->op == "*",
        "the inner operation was not a multiplication"
    );

    auto bare = std::move(parseOne("let x;"));
    const auto* withoutValue = dynamic_cast<LetStmt*>(bare.get());

    expectTrue(
        "a let may have no value",
        withoutValue != nullptr && withoutValue->initializer == nullptr,
        "a value was expected"
    );

    auto choice = std::move(parseOne("switch x { case 1: print(1); break; default: print(2); }"));
    const auto* branch = dynamic_cast<const SwitchStmt*>(choice.get());

    expectTrue(
        "a switch keeps its subject",
        branch != nullptr &&
            expressionName(*branch->subject) == "name",
        "the subject was lost"
    );

    expectTrue(
        "a switch keeps its cases in order",
        branch != nullptr &&
            branch->cases.size() == 2 &&
            branch->cases[0].value != nullptr &&
            branch->cases[1].value == nullptr,
        "the cases are wrong"
    );

    expectTrue(
        "a default case has no value",
        branch != nullptr && branch->cases[1].body != nullptr,
        "the default body was lost"
    );

    auto emptyCase = std::move(parseOne("switch x { case 1: case 2: }"));
    const auto* thin = dynamic_cast<const SwitchStmt*>(emptyCase.get());

    const auto* thinBody = thin == nullptr ?
        nullptr : dynamic_cast<const BlockStmt*>(thin->cases[0].body.get());

    expectTrue(
        "a case can be empty",
        thin != nullptr &&
            thin->cases.size() == 2 &&
            thinBody != nullptr &&
            thinBody->statements.empty(),
        "an empty case was not kept empty"
    );

    auto loop = std::move(parseOne("for (let v in xs) { }"));
    const auto* forIn = dynamic_cast<const ForInStmt*>(loop.get());

    expectTrue(
        "a for in keeps its variable",
        forIn != nullptr && forIn->name == "v",
        "the variable was not kept"
    );

    auto header = std::move(parseOne("for (let i = 0; i < 9; i = i + 1) { }"));
    const auto* counted = dynamic_cast<const ForStmt*>(header.get());

    expectTrue(
        "a for keeps its parts",
        counted != nullptr &&
            counted->initializer != nullptr &&
            counted->condition != nullptr &&
            counted->increment != nullptr,
        "a part of the loop header was lost"
    );
}

void errorTests() {
    expectError("a missing semicolon", "let x = 1", "expected ';'");
    expectError("a missing name", "let = 1;", "expected a variable name");
    expectError("a number as a name", "let 5 = 1;", "expected a variable name");
    expectError("a missing close", "print(1;", "expected ')'");
    expectError("a missing close brace", "{ print(1);", "expected '}'");
    expectError("a missing close bracket", "let x = [1;", "expected ']'");
    expectError("a map without a colon", "let x = {\"a\" 1};", "expected ':'");
    expectError("an empty switch", "switch 1 { }", "at least one case");
    expectError(
        "a switch without a brace",
        "switch 1 case 1: ",
        "expected '{'"
    );
    expectError(
        "a switch case without a colon",
        "switch 1 { case 1 print(1); }",
        "expected ':' after a case value"
    );
    expectError(
        "a switch default without a colon",
        "switch 1 { default print(1); }",
        "expected ':' after default"
    );
    expectError(
        "two defaults in one switch",
        "switch 1 { default: default: }",
        "already has a default"
    );
    expectError(
        "a statement that is not a case",
        "switch 1 { print(1); }",
        "expected 'case' or 'default'"
    );
    expectError("a switch without a subject", "switch { case 1: }", "expected an expression");
    expectError("a map without a value", "let x = {\"a\": };", "expected an expression");
    expectError("a duplicated name", "let let = 1;", "expected a variable name");
    expectError("an empty function", "function f() { }", "empty body");
    expectError("a function without a name", "function (a) { return a; }", "expected a function name");
    expectError("a duplicated parameter", "function f(a, a) { return a; }", "duplicate parameter");
    expectError("a bad assignment target", "1 = 2;", "cannot be assigned to");
    expectError("a missing loop name", "for (in [1]) { }", "expected an expression");
    expectError("a list with two values and no comma", "let x = [1 2];", "expected ']'");
    expectError("an empty hole", "print(\"{}\");", "empty '{}' to fill");
    expectError(
        "a hole with no close",
        "print(\"{x\");",
        "unfinished string literal"
    );
    expectError("a close with no hole", "print(\"}\" + \"x\");", "nothing to fill");
    expectError("a hole with a bad expression", "print(\"{1 2}\");", "cannot read the expression");
    expectError("a for in without a list", "for (let v in) { }", "expected an expression");
    expectError("a stray closing brace", "}", "expected an expression, found '}'");
    expectError("a stray closing paren", ")", "expected an expression, found ')'");
    expectError("a stray closing bracket", "]", "expected an expression, found ']'");
}

} 

int main() {
    shapeTests();
    destructureTests();
    expressionTests();
    detailTests();
    errorTests();

    if (failures == 0) {
        std::cout << "all " << checks << " parser checks passed\n";
        return 0;
    }

    std::cout << failures << " parser checks failed\n";
    return 1;
}