#include "compiler/compiler.h"
#include "debug/disassembler.h"
#include "errors/errors.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "runtime/object_function.h"
#include "stdlib/stdlib.h"
#include "vm/vm.h"

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
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

template <typename Error>
void expectError(
    const std::string& name,
    const std::string& source,
    const std::string& expected,
    bool expectLine = false,
    int line = 0
) {
    try {
        Parser parser(Lexer(source).tokenize());
        Compiler compiler;
        compiler.compile(parser.parse(), "test");
    } catch (const Error& problem) {
        std::string detail = problem.what();

        if (detail.find(expected) == std::string::npos) {
            expectTrue(name, false, "message was '" + detail + "'");
            return;
        }

        if (expectLine) {
            expectTrue(
                name,
                problem.location().line == line,
                "line was " + std::to_string(problem.location().line)
            );
            return;
        }

        expectTrue(name, true, "");
        return;
    } catch (const std::exception& problem) {
        expectTrue(name, false, std::string("wrong error: ") + problem.what());
        return;
    }

    expectTrue(name, false, "no error was raised");
}

std::string runSource(const std::string& source) {
    std::stringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());

    try {
        Parser parser(Lexer(source).tokenize());
        Compiler compiler;

        VM vm;
        NovaStd::registerBuiltins(vm);

        vm.interpret(compiler.compile(parser.parse(), "test"));
    } catch (...) {
        std::cout.rdbuf(original);
        throw;
    }

    std::cout.rdbuf(original);

    return captured.str();
}

void expectOutput(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    std::string output;

    try {
        output = runSource(source);
    } catch (const std::exception& problem) {
        failures++;
        std::cout << "FAILED " << name << ": " << problem.what() << "\n";
        return;
    }

    if (output != expected) {
        failures++;
        std::cout << "FAILED " << name << ": got '" << output
                  << "' expected '" << expected << "'\n";
    }
}

void expectRuntimeError(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    try {
        runSource(source);
    } catch (const RuntimeError& problem) {
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

void lexerTests() {
    expectError<LexerError>("bad character", "let x = 1 @ 2;", "unexpected character");
    expectError<LexerError>("unterminated string", "let x = \"open;", "unfinished string literal");
    expectError<LexerError>("unterminated comment", "/* open", "unfinished block comment");
    expectError<LexerError>("bad escape", "let x = \"a\\qb\";", "unknown escape");
    expectError<ParserError>("number followed by letters", "let x = 12ab;", "expected ';'");
}

void parserTests() {
    expectError<ParserError>("missing semicolon", "let x = 1\nlet y = 2;", "expected ';'");
    expectError<ParserError>("missing paren", "print(1;", "expected ')'");
    expectError<ParserError>("missing brace", "if true { print(1);", "expected '}'");
    expectError<ParserError>("unknown variable statement", "1 + 2 3;", "expected ';'");
    expectError<ParserError>("bad assignment target", "1 = 2;", "cannot be assigned to");
    expectError<ParserError>("duplicate parameter", "function f(a, a) { return a; }", "duplicate parameter name");
    expectError<ParserError>("empty function body", "function f() { }", "empty body");
    expectError<ParserError>("missing name", "function () { return 1; }", "expected a function name");
    expectError<ParserError>("bad for in header", "for (let i in) { }", "expected an expression");
    expectError<ParserError>("unterminated list", "let x = [1, 2;", "expected ']'");
    expectError<ParserError>("empty switch", "switch 1 { }", "at least one case");
    expectError<ParserError>("switch without a brace", "switch 1 case 1:", "expected '{'");
    expectError<ParserError>("case without a colon", "switch 1 { case 1 }", "expected ':' after a case value");
    expectError<ParserError>("two defaults", "switch 1 { default: default: }", "already has a default");
    expectError<ParserError>("statement in a switch", "switch 1 { print(1); }", "expected 'case' or 'default'");
    expectError<ParserError>("empty hole", "print(\"{}\");", "empty '{}' to fill");
    expectError<LexerError>("a hole with no close", "print(\"{x\");", "unfinished string literal");
    expectError<ParserError>("a close with no hole", "print(\"}\" + \"a\");", "nothing to fill");
    expectError<ParserError>("a hole with bad text", "print(\"{1 2}\");", "cannot read the expression");
    expectError<ParserError>("error line number", "\n\nlet = 5;", "after 'let'", true, 3);
}

void compilerTests() {
    expectError<CompileError>("break outside loop", "break;", "'break' is only allowed inside a loop or a switch");
    expectError<CompileError>("continue outside loop", "continue;", "'continue' is only allowed inside a loop");

    std::string many = "function f() {";

    for (int i = 0; i < 300; ++i) {
        many += " let v" + std::to_string(i) + " = " + std::to_string(i) + ";";
    }

    many += " return 1; } f();";

    expectError<CompileError>("too many locals", many, "too many local variables");
}

void runtimeTests() {
    expectRuntimeError("call a number", "let x = 1; x();", "not a function");
    expectRuntimeError("unknown variable", "print(missing);", "unknown variable 'missing'");
    expectRuntimeError("wrong argument count", "function f(a) { return a; } f(1, 2);", "expects 1 arguments");
    expectRuntimeError("index a number", "let x = 1; print(x[0]);", "cannot index a value of type number");
    expectRuntimeError("list index range", "let x = [1]; print(x[5]);", "out of range");
    expectRuntimeError("string index range", "print(\"ab\"[9]);", "out of range");
    expectRuntimeError("map key type", "let m = {}; m[[1]] = 2;", "map keys must be string, number or bool");
    expectRuntimeError("method on number", "let x = 1; x.push(2);", "number has no method 'push'");
    expectRuntimeError("string has no push", "\"a\".push(2);", "string has no method 'push'");
    expectRuntimeError("subtract a string", "print(\"a\" - 1);", "cannot use - with string and number");
    expectRuntimeError("compare string and number", "print(\"a\" < 1);", "cannot compare string with number using <");
    expectRuntimeError("divide by zero", "print(1 / 0);", "division by zero");
    expectRuntimeError("modulo by zero", "print(1 % 0);", "remainder by zero");
    expectRuntimeError("negate a string", "print(-\"a\");", "cannot negate");
    expectRuntimeError("pop empty list", "let x = []; x.pop();", "empty list");
    expectRuntimeError("number has no len", "print(1.len());", "number has no method 'len'");
    expectRuntimeError("range step zero", "range(1, 2, 0);", "cannot be zero");
    expectRuntimeError("deep recursion", "function f(n) { return f(n + 1); } f(0);", "too much recursion");
    expectRuntimeError("error builtin", "error(\"boom\");", "boom");
    expectRuntimeError("assert builtin", "assert(false, \"nope\");", "nope");
    expectRuntimeError("list set on string", "let s = \"a\"; s[0] = \"b\";", "strings cannot be changed");
    expectRuntimeError(
        "property set on a list",
        "let x = [1]; x.a = 2;",
        "cannot set field 'a' on a value of type list"
    );
    expectRuntimeError(
        "property set on a string",
        "\"a\".a = 2;",
        "cannot set field 'a' on a value of type string"
    );
    expectRuntimeError("call a missing key", "let m = {}; m.nope();", "not a function");
    expectRuntimeError("missing list method", "[1].nope();", "list has no method 'nope'");
    expectRuntimeError("min on an empty list", "[].min();", "empty list");
    expectRuntimeError("toNumber on text", "\"abc\".toNumber();", "as a number");
}

void networkTests() {
    expectRuntimeError(
        "ping needs a string",
        "ping(5);",
        "ping expects a string"
    );
    expectRuntimeError(
        "ping with a tiny timeout",
        "ping(\"192.0.2.1\", 0);",
        "timeout of one millisecond"
    );
    expectRuntimeError(
        "ping with a long timeout",
        "ping(\"192.0.2.1\", 999999);",
        "at most 60000"
    );
    expectRuntimeError(
        "ping with a bad port",
        "ping(\"192.0.2.1\", 500, 70000);",
        "port between 0 and 65535"
    );
    expectRuntimeError(
        "scan with a bad range",
        "scan(\"192.0.2.\", 5, 900);",
        "range between 0 and 255"
    );
    expectRuntimeError(
        "scan with a bad prefix",
        "scan(\"not a prefix\", 1, 2);",
        "needs a prefix"
    );
    expectRuntimeError(
        "scanDetail with a bad timeout",
        "scanDetail(\"192.0.2.\", 1, 2, 0);",
        "timeout between 1 and 60000"
    );
    expectRuntimeError(
        "get over https",
        "get(\"https://example.com/\");",
        "does not speak https"
    );
    expectRuntimeError(
        "get with an unknown scheme",
        "get(\"ftp://example.com/\");",
        "http:// addresses only"
    );
    expectRuntimeError(
        "get without a host",
        "get(\"http:///x\");",
        "needs a host name"
    );
    expectRuntimeError(
        "readFile needs a string",
        "readFile(7);",
        "readFile expects a string"
    );
    expectRuntimeError(
        "read a missing file",
        "readFile(\"/tmp/nova_missing_file.txt\");",
        "cannot read"
    );
    expectRuntimeError(
        "write to a bad path",
        "writeFile(\"\", \"x\");",
        "cannot write"
    );

    expectOutput(
        "an unroutable address stays down",
        "print(ping(\"192.0.2.1\", 120));",
        "false\n"
    );
    expectOutput(
        "loopback answers",
        "print(ping(\"127.0.0.1\", 500));",
        "true\n"
    );
    expectOutput(
        "lookup of localhost",
        "print(lookup(\"localhost\").contains(\"127.0.0.1\"));",
        "true\n"
    );
    expectOutput(
        "lookup of a name that does not exist",
        "print(len(lookup(\"no-such-host.invalid\")));",
        "0\n"
    );
    expectOutput(
        "scan of an unused range",
        "print(len(scan(\"192.0.2.\", 10, 12, 120)));",
        "0\n"
    );
    expectOutput(
        "file round trip",
        "let p = \"/tmp/nova_diag_test.txt\";"
        " writeFile(p, \"a\\nb\\n\");"
        " appendFile(p, \"c\\n\");"
        " print(len(readFile(p).trim().split(\"\\n\")));"
        " print(readFile(p).split(\"\\n\")[1]);"
        " print(fileExists(p));"
        " print(removeFile(p));"
        " print(fileExists(p));",
        "3\nb\ntrue\ntrue\nfalse\n"
    );
    expectOutput(
        "removeFile on a missing file",
        "print(removeFile(\"/tmp/nova_diag_missing.txt\"));",
        "false\n"
    );
}

void outputTests() {
    expectOutput("print newline", "print(\"a\");", "a\n");
    expectOutput("print many values", "print(\"a\", 1, true);", "a 1 true\n");
    expectOutput("print list", "print([1, 2]);", "[1, 2]\n");
    expectOutput("print nested list", "print([[1], [2, 3]]);", "[[1], [2, 3]]\n");
    expectOutput("print map", "print({\"a\": 1});", "{\"a\": 1}\n");
    expectOutput("print nested map", "print({\"a\": {\"b\": 2}});", "{\"a\": {\"b\": 2}}\n");
    expectOutput("print function", "print(print);", "<native print>\n");
    expectOutput("numbers keep decimals", "print(1.5);", "1.5\n");
    expectOutput("number plus string", "print(1 + \"a\");", "1a\n");
    expectOutput("string plus list is an error path", "print(\"a\" + [1]);", "a[1]\n");
    expectOutput("big numbers use exponents", "print(100000000000000000000);", "1e+20\n");
    expectOutput("nil prints", "print(nil);", "nil\n");
    expectOutput("comments are ignored", "// one\n/* two */\nprint(1);", "1\n");
    expectOutput("assignment returns value", "let x = 0; print(x = 5);", "5\n");
    expectOutput("chained calls", "print(str(len(\"abcd\")));", "4\n");
    expectOutput("globals survive statements", "let a = 1; let b = a + 1; print(b);", "2\n");
    expectOutput("shadowing in block", "let a = 1; { let a = 2; print(a); } print(a);", "2\n1\n");
    expectOutput("closure keeps its own state", "function c() { let n = 0; return function() { n = n + 1; return n; }; } let a = c(); let b = c(); a(); print(a()); print(b());", "2\n1\n");
    expectOutput("recursion depth is fine", "function f(n) { if n == 0 { return 0; } return f(n - 1) + 1; } print(f(120));", "120\n");
    expectOutput("while with continue", "let i = 0; let s = 0; while i < 5 { i = i + 1; if i == 3 { continue; } s = s + i; } print(s);", "12\n");
    expectOutput("for loop empty body braces", "let s = 0; for (let i = 0; i < 3; i = i + 1) { s = s + 1; } print(s);", "3\n");
    expectOutput("nested functions see each other", "function outer() { function inner() { return 7; } return inner(); } print(outer());", "7\n");
    expectOutput("map literal with expressions", "let n = 2; let m = {\"v\": n * 3}; print(m[\"v\"]);", "6\n");
    expectOutput("method chaining", "print(\"a-b-c\".split(\"-\").map(function(x) { return x.upper(); }).join(\"+\"));", "A+B+C\n");
    expectOutput("property assignment", "let m = {\"a\": 1}; m.b = 2; print(m.keys().join(\",\"), m.b);", "a,b 2\n");
    expectOutput("property read of a missing key", "let m = {}; print(m.nothing == nil);", "true\n");
    expectOutput("map keys without quotes", "let m = {name: \"ada\", age: 36}; print(m.name, m.age);", "ada 36\n");
    expectOutput("list methods that copy", "let n = [3, 1, 2]; print(n.sort().join(\",\"), n.join(\",\"), n.reverse().join(\",\"), n.join(\",\"));", "1,2,3 3,1,2 2,1,3 3,1,2\n");
    expectOutput("list methods that change", "let n = [1, 2]; n.push(3); n.insert(0, 0); print(n.join(\",\"), n.pop(), n.join(\",\"));", "0,1,2,3 3 0,1,2\n");
    expectOutput("number formatting", "print(1, 1.5, 0.25, 100000000000000000000, 1e21);", "1 1.5 0.25 1e+20 1e+21\n");
    expectOutput("escapes in output", "print(\"a\\tb\", \"c\\nd\");", "a\tb c\nd\n");
    expectOutput("index assignment returns value", "let x = [1]; print(x[0] = 9);", "9\n");
    expectOutput("compound assignment returns nothing", "let x = 1; x += 1; print(x);", "2\n");
}

} 

int main() {
    lexerTests();
    parserTests();
    compilerTests();
    runtimeTests();
    networkTests();
    outputTests();

    if (failures == 0) {
        std::cout << "all " << checks << " checks passed\n";
        return 0;
    }

    std::cout << failures << " checks failed\n";
    return 1;
}
