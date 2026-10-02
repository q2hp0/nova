#include "errors/errors.h"
#include "lexer/lexer.h"

#include <iostream>
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

std::vector<Token> scan(const std::string& source) {
    return Lexer(source).tokenize();
}

std::string shape(const std::vector<Token>& tokens) {
    std::string text;

    for (size_t i = 0; i < tokens.size(); i++) {
        if (i > 0) {
            text += " ";
        }

        text += tokenTypeName(tokens[i].type);

        if (i + 1 < tokens.size()) {
            text += "(" + tokens[i].text + ")";
        }
    }

    return text;
}

void expectShape(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    std::string actual;

    try {
        actual = shape(scan(source));
    } catch (const std::exception& problem) {
        failures++;
        std::cout << "FAILED " << name << ": " << problem.what() << "\n";
        return;
    }

    if (actual != expected) {
        failures++;
        std::cout << "FAILED " << name << "\n  expected: " << expected
                  << "\n  actual:   " << actual << "\n";
    }
}

void expectError(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    try {
        scan(source);
    } catch (const LexerError& problem) {
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

void numberTests() {
    expectShape("a whole number", "42", "number(42) end of file");
    expectShape("a decimal", "3.5", "number(3.5) end of file");
    expectShape("a trailing dot", "7.", "number(7) .(.) end of file");
    expectShape("a leading dot", ".5", ".(.) number(5) end of file");
    expectShape(
        "two dots in a row",
        "1.2.3",
        "number(1.2) .(.) number(3) end of file"
    );
    expectShape("a rest marker", "...", "...(...) end of file");
    expectShape(
        "a rest marker in a list",
        "[a, ...b]",
        "[([) identifier(a) ,(,) ...(...) identifier(b) ](]) end of file"
    );
    expectShape(
        "four dots",
        "....",
        "...(...) .(.) end of file"
    );
    expectShape("an exponent", "1e3", "number(1e3) end of file");
    expectShape("a small exponent", "1.5e-2", "number(1.5e-2) end of file");

    auto exponent = scan("1e3");
    expectTrue(
        "an exponent keeps its value",
        exponent[0].number == 1000.0,
        "was " + std::to_string(exponent[0].number)
    );

    auto tokens = scan("2.75");
    expectTrue(
        "the number keeps its value",
        tokens[0].number == 2.75,
        "was " + std::to_string(tokens[0].number)
    );

    auto zeros = scan("0.5");
    expectTrue(
        "a zero fraction keeps its value",
        zeros[0].number == 0.5,
        "was " + std::to_string(zeros[0].number)
    );
}

void stringTests() {
    expectShape("a plain string", "\"hi\"", "string(hi) end of file");
    expectShape(
        "a newline escape",
        "\"a\\nb\"",
        "string(a\nb) end of file"
    );
    expectShape(
        "a quote escape",
        "\"a\\\"b\"",
        "string(a\"b) end of file"
    );
    expectShape(
        "a backslash escape",
        "\"a\\\\b\"",
        "string(a\\b) end of file"
    );

    auto tab = scan("\"a\\tb\"");
    expectTrue(
        "an escape becomes a real character",
        tab[0].text == "a\tb",
        "was " + tab[0].text
    );

    auto single = scan("\"a\\'b\"");
    expectTrue(
        "a single quote can be escaped",
        single.size() == 2 &&
            single[0].type == TokenType::String &&
            single[0].text == "a'b",
        "the escape was not read"
    );

    auto escape = scan("\"a\\eb\"");
    expectTrue(
        "the escape e is an escape character",
        escape[0].text == std::string("a\x1b") + "b",
        "the escape was not read"
    );

    auto zero = scan("\"a\\0b\"");
    expectTrue(
        "the escape 0 is a zero byte",
        zero[0].text == std::string("a\0b", 3),
        "the escape was not read"
    );

    auto empty = scan("\"\"");
    expectTrue(
        "an empty string is allowed",
        empty[0].text.empty(),
        "was not empty"
    );

    expectShape(
        "a hole in a string",
        "\"a{b}c\"",
        "string(a\x01" "b\x02" "c) end of file"
    );

    expectShape(
        "an escaped brace is not a hole",
        "\"a\\{b\\}c\"",
        "string(a{b}c) end of file"
    );

    expectShape(
        "a nested string keeps its text",
        "\"a{ \"b\" }c\"",
        "string(a\x01" " \"b\" " "\x02" "c) end of file"
    );

    expectError("an unknown escape", "\"a\\qb\"", "unknown escape");
    expectError("a string that never ends", "\"open", "unfinished string");
    expectError("a newline inside a string", "\"a\nb\"", "unfinished string");
}

void commentTests() {
    expectShape(
        "a line comment",
        "// hello\n1",
        "number(1) end of file"
    );
    expectShape(
        "a block comment",
        "/* hello */ 1",
        "number(1) end of file"
    );
    expectShape(
        "a block comment over lines",
        "/* a\nb */ 1",
        "number(1) end of file"
    );
    expectShape(
        "a comment at the end",
        "1 // hello",
        "number(1) end of file"
    );
    expectError(
        "a block comment that never ends",
        "/* open",
        "unfinished block comment"
    );
}

void keywordTests() {
    expectShape(
        "every keyword",
        "let if else while for in switch case default "
        "function return break continue and or not true false nil null",
        "let(let) if(if) else(else) while(while) for(for) in(in) "
        "switch(switch) case(case) default(default) "
        "function(function) return(return) break(break) continue(continue) "
        "and(and) or(or) not(not) true(true) false(false) nil(nil) "
        "nil(null) end of file"
    );
    expectShape(
        "a name that looks like a keyword",
        "letter",
        "identifier(letter) end of file"
    );
    expectShape(
        "a keyword inside a longer name",
        "forever",
        "identifier(forever) end of file"
    );
    expectShape(
        "the class and object keywords",
        "class new this super",
        "class(class) new(new) this(this) super(super) end of file"
    );
    expectShape(
        "the error keywords",
        "throw try catch finally",
        "throw(throw) try(try) catch(catch) finally(finally) end of file"
    );
    expectShape(
        "the module keywords",
        "import export from as",
        "import(import) export(export) from(from) as(as) end of file"
    );
}

void operatorTests() {
    expectShape(
        "arithmetic",
        "+ - * / %",
        "+(+) -(-) *(*) /(/) %(%) end of file"
    );
    expectShape(
        "compound assignment",
        "+= -= *= /= %=",
        "+=(+=) -=(-=) *=(*=) /=(/=) %=(%=) end of file"
    );
    expectShape(
        "equality",
        "== != =",
        "==(==) !=(!=) =(=) end of file"
    );
    expectShape(
        "ordering",
        "< <= > >=",
        "<(<) <=(<=) >(>) >=(>=) end of file"
    );
    expectShape(
        "brackets and commas",
        "( ) { } [ ] , . : ;",
        "((() )()) {({) }(}) [([) ](]) ,(,) .(.) :(:) ;(;) end of file"
    );
    expectShape(
        "a not equal sign",
        "!",
        "!(!) end of file"
    );
}

void positionTests() {
    auto tokens = scan("let x = 1;\nlet y = 2;");

    expectTrue(
        "the first token is on line one",
        tokens[0].line == 1 && tokens[0].column == 1,
        "was line " + std::to_string(tokens[0].line) + " column " +
        std::to_string(tokens[0].column)
    );

    expectTrue(
        "the second statement moves to line two",
        tokens[5].line == 2 && tokens[5].column == 1,
        "was line " + std::to_string(tokens[5].line) + " column " +
        std::to_string(tokens[5].column)
    );

    auto inner = scan("  let x");
    expectTrue(
        "a column follows the spaces",
        inner[0].column == 3,
        "was " + std::to_string(inner[0].column)
    );

    auto afterBlock = scan("/* a\nb\nc */ let");
    expectTrue(
        "a block comment moves the line",
        afterBlock[0].line == 3,
        "was " + std::to_string(afterBlock[0].line)
    );

    expectError(
        "an error knows its column",
        "1 @",
        "line 1:3"
    );

    expectError(
        "an error on a later line knows its line",
        "\n\nlet x = ?;",
        "line 3:9"
    );
}

void unknownTests() {
    expectError("a question mark", "?", "unexpected character '?'");
    expectError("a hash", "#", "unexpected character '#'");
    expectError("a backtick", "`", "unexpected character '`'");
    expectError("an ampersand", "&", "unexpected character '&'");
}

} 

int main() {
    numberTests();
    stringTests();
    commentTests();
    keywordTests();
    operatorTests();
    positionTests();
    unknownTests();

    if (failures == 0) {
        std::cout << "all " << checks << " lexer checks passed\n";
        return 0;
    }

    std::cout << failures << " lexer checks failed\n";
    return 1;
}