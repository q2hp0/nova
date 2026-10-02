#include "compiler/compiler.h"
#include "errors/errors.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/object_function.h"
#include "runtime/object_list.h"
#include "runtime/object_map.h"
#include "runtime/upvalue.h"
#include "stdlib/stdlib.h"
#include "vm/stack.h"
#include "vm/vm.h"

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

ValueStack stackOf(std::initializer_list<double> numbers) {
    ValueStack stack;

    for (double number : numbers) {
        stack.push(Value(number));
    }

    return stack;
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

void expectErrorText(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    checks++;

    try {
        runSource(source);
    } catch (const std::exception& problem) {
        std::string detail = problem.what();

        if (detail.find(expected) == std::string::npos) {
            failures++;
            std::cout << "FAILED " << name << ": message was '"
                      << detail << "'\n";
        }

        return;
    }

    failures++;
    std::cout << "FAILED " << name << ": no error was raised\n";
}

void expectRuntimeError(
    const std::string& name,
    const std::string& source,
    const std::string& expected
) {
    expectErrorText(name, source, expected);
}

void valueTests() {
    Value number(2.5);
    Value text("nova");
    Value flag(true);
    Value nothing;

    expectTrue("a number is a number", number.isNumber(), "wrong type");
    expectTrue("text is text", text.isString(), "wrong type");
    expectTrue("a flag is a flag", flag.isBool(), "wrong type");
    expectTrue("nothing is nil", nothing.isNil(), "wrong type");
    expectTrue("a number reads back", number.asNumber() == 2.5, "wrong value");
    expectTrue("text reads back", text.asString() == "nova", "wrong value");
    expectTrue("nil is false", !nothing.isTruthy(), "nil should be false");
    expectTrue("false is false", !Value(false).isTruthy(), "false should be false");
    expectTrue("zero is true", Value(0.0).isTruthy(), "zero should be true");
    expectTrue("empty text is true", Value("").isTruthy(), "text should be true");

    expectTrue(
        "a number is called number",
        Value(1).typeName() == "number",
        Value(1).typeName()
    );

    expectTrue(
        "two numbers are equal",
        Value(3).equals(Value(3.0)),
        "3 should equal 3"
    );

    expectTrue(
        "a number is not text",
        !Value(3).equals(Value("3")),
        "3 should not equal '3'"
    );

    auto first = std::make_shared<ListObject>(
        std::vector<Value>{Value(1.0), Value(2.0)}
    );
    auto same = first;
    auto other = std::make_shared<ListObject>(
        std::vector<Value>{Value(1.0), Value(2.0)}
    );

    expectTrue(
        "a list equals itself",
        Value(first).equals(Value(same)),
        "the same list was not equal to itself"
    );

    expectTrue(
        "two equal lists are still two values",
        !Value(first).equals(Value(other)),
        "separate lists were treated as one"
    );

    auto left = std::make_shared<MapObject>();
    auto right = std::make_shared<MapObject>();

    expectTrue(
        "two empty maps are still two values",
        !Value(left).equals(Value(right)),
        "separate maps were treated as one"
    );

    expectTrue(
        "a map equals itself",
        Value(left).equals(Value(left)),
        "the same map was not equal to itself"
    );

    expectTrue(
        "a list is not equal to a map",
        !Value(first).equals(Value(left)),
        "a list matched a map"
    );

    expectTrue(
        "an empty list is not equal to a missing value",
        !Value(first).equals(Value()),
        "a list matched nil"
    );

    expectTrue(
        "text shows quotes when inspected",
        Value("a").inspect() == "\"a\"",
        Value("a").inspect()
    );

    expectTrue(
        "nil shows as nil",
        Value().inspect() == "nil",
        Value().inspect()
    );

    expectTrue(
        "a whole number prints without a point",
        formatNumber(2) == "2",
        formatNumber(2)
    );

    expectTrue(
        "a fraction prints with a point",
        formatNumber(2.5) == "2.5",
        formatNumber(2.5)
    );
}

void stackTests() {
    ValueStack stack = stackOf({1, 2, 3});

    expectTrue("a stack knows its size", stack.size() == 3, "size was 3?");
    expectTrue("the top is the last value", stack.top().asNumber() == 3.0, "wrong top");
    expectTrue("a slot can be read", stack.at(0).asNumber() == 1.0, "wrong slot");

    stack.set(0, Value(9));
    expectTrue("a slot can be written", stack.at(0).asNumber() == 9.0, "not written");

    Value taken = stack.pop();
    expectTrue("pop takes the top", taken.asNumber() == 3.0, "wrong value popped");
    expectTrue("pop shrinks the stack", stack.size() == 2, "size was not 2");

    stack.truncate(1);
    expectTrue("truncate cuts the top", stack.size() == 1, "size was not 1");
    expectTrue("truncate keeps the bottom", stack.at(0).asNumber() == 9.0, "wrong value");

    stack.push(Value(5));
    stack.clear();
    expectTrue("clear empties the stack", stack.empty(), "stack was not empty");

    stack.push(Value(1));
    expectTrue("data points at the first slot", stack.data()->asNumber() == 1.0, "wrong slot");

    bool poppedTooMuch = false;

    try {
        stack.pop();
        stack.pop();
    } catch (const std::exception&) {
        poppedTooMuch = true;
    }

    expectTrue("pop on an empty stack fails", poppedTooMuch, "no error was raised");

    bool readTooMuch = false;

    try {
        stack.at(99);
    } catch (const std::exception&) {
        readTooMuch = true;
    }

    expectTrue("reading past the end fails", readTooMuch, "no error was raised");

    ValueStack big;

    for (size_t i = 0; i < 1000; i++) {
        big.push(Value(static_cast<double>(i)));
    }

    expectTrue("a big stack keeps every value", big.at(999).asNumber() == 999.0, "lost a value");
}

void upvalueTests() {
    Value cell(7);
    auto upvalue = std::make_shared<Upvalue>(&cell);

    expectTrue("an upvalue reads its cell", upvalue->get().asNumber() == 7.0, "wrong value");
    expectTrue("an open upvalue is open", upvalue->isOpen(), "it should be open");

    upvalue->close();
    expectTrue("closing an upvalue keeps the value", upvalue->get().asNumber() == 7.0, "value lost");
    expectTrue("a closed upvalue is not open", !upvalue->isOpen(), "it should be closed");

    cell = Value(9);
    expectTrue("a closed upvalue does not follow the cell", upvalue->get().asNumber() == 7.0, "it followed");
}

void callTests() {
    expectOutput(
        "a call leaves nothing behind",
        "function f() { return nil; } f(); print(\"clean\");",
        "clean\n"
    );

    expectOutput(
        "an argument expression runs first",
        "function twice(n) { return n * 2; } function give() { print(\"arg\"); return 3; } print(twice(give()));",
        "arg\n6\n"
    );

    expectOutput(
        "a method receives its own list",
        "print([1, 2].len());",
        "2\n"
    );

    expectOutput(
        "a method can be held",
        "let join = [1, 2].join; print(join(\"-\"));",
        "1-2\n"
    );

    expectOutput(
        "a function can return many things in a list",
        "function pair() { return [1, 2]; } let out = pair(); print(out[1]);",
        "2\n"
    );

    expectOutput(
        "deep recursion works",
        "function down(n) { if n == 0 { return \"end\"; } return down(n - 1); } print(down(300));",
        "end\n"
    );

    expectRuntimeError(
        "too much recursion stops the program",
        "function down(n) { return down(n + 1); } down(0);",
        "too much recursion"
    );

    expectRuntimeError(
        "the wrong number of arguments stops the program",
        "function f(a, b) { return a; } f(1);",
        "expects 2 arguments"
    );

    expectRuntimeError(
        "too many arguments stop the program",
        "function f(a) { return a; } f(1, 2);",
        "expects 1 arguments"
    );

    expectRuntimeError(
        "calling nothing stops the program",
        "let nothing = nil; nothing();",
        "not a function"
    );
}

void valueSharingTests() {
    expectOutput(
        "a list copied by copy() is separate",
        "let a = [1]; let b = a.copy(); b.push(2); print(a.len(), b.len());",
        "1 2\n"
    );

    expectOutput(
        "a list kept in a map is the same list",
        "let holder = {items: [1]}; holder.items.push(2); print(holder.items.len());",
        "2\n"
    );

    expectOutput(
        "a map returned from a function stays alive",
        "function make() { let m = {}; m.value = 5; return m; } print(make().value);",
        "5\n"
    );

    expectOutput(
        "a list built in a loop does not mix up",
        "let out = []; for (let i in range(3)) { out.push(i * 2); } print(out.join(\",\"));",
        "0,2,4\n"
    );

    expectOutput(
        "a string cannot be changed",
        "print((\"ab\" + \"cd\"));",
        "abcd\n"
    );
}

void interpolationTests() {
    expectOutput(
        "a hole becomes text",
        "let name = \"nova\"; print(\"hi {name}\", 1);",
        "hi nova 1\n"
    );

    expectOutput(
        "a number in a hole becomes text",
        "print(\"{1 + 1}\");",
        "2\n"
    );

    expectOutput(
        "a list in a hole keeps its brackets",
        "print(\"{[1, 2]}\");",
        "[1, 2]\n"
    );

    expectOutput(
        "a hole can call a method",
        "print(\"{[3, 1].sort().join(\"-\")}\");",
        "1-3\n"
    );

    expectOutput(
        "a hole can read a property",
        "print(\"{{a: 7}.a}\");",
        "7\n"
    );

    expectOutput(
        "a hole can hold a string",
        "let who = \"nova\"; print(\"{ \"hi \" + who }\");",
        "hi nova\n"
    );

    expectOutput(
        "braces can be escaped",
        "print(\"a \\{b\\} c\");",
        "a {b} c\n"
    );

    expectOutput(
        "text after a hole survives",
        "print(\"{1 + 1} apples\");",
        "2 apples\n"
    );

    expectOutput(
        "two holes in one string",
        "print(\"{1}-{2}\", \"x\");",
        "1-2 x\n"
    );

    expectErrorText(
        "an empty hole is refused",
        "print(\"{}\");",
        "empty '{}' to fill"
    );

    expectErrorText(
        "a close with no hole is refused",
        "print(\"}\" + \"x\");",
        "nothing to fill"
    );

    expectErrorText(
        "a hole with no close is refused",
        "print(\"{1 + 1\");",
        "unfinished string literal"
    );

    expectErrorText(
        "a hole with text after the expression is refused",
        "print(\"{1 2}\");",
        "extra text"
    );

    expectErrorText(
        "a hole with an unknown name stops the program",
        "print(\"{missingName}\");",
        "unknown variable 'missingName'"
    );
}

void switchTests() {
    expectOutput(
        "a switch runs the case that matches",
        "switch 2 { case 1: print(\"a\"); case 2: print(\"b\"); }",
        "b\n"
    );

    expectOutput(
        "a switch falls through",
        "switch 1 { case 1: print(\"a\"); case 2: print(\"b\"); }",
        "a\nb\n"
    );

    expectOutput(
        "a switch break stops the fall through",
        "switch 1 { case 1: print(\"a\"); break; case 2: print(\"b\"); }",
        "a\n"
    );

    expectOutput(
        "a switch default runs when nothing matches",
        "switch 9 { case 1: print(\"a\"); default: print(\"d\"); }",
        "d\n"
    );

    expectOutput(
        "a switch with no match and no default runs nothing",
        "switch 9 { case 1: print(\"a\"); } print(\"end\");",
        "end\n"
    );

    expectOutput(
        "a switch default in the middle still falls through",
        "switch 9 { default: print(\"d\"); case 1: print(\"a\"); }",
        "d\na\n"
    );

    expectOutput(
        "a switch keeps its value out of the names",
        "let subject = 1; switch 2 { case 2: let subject = 9; print(subject); } print(subject);",
        "9\n1\n"
    );

    expectOutput(
        "a switch case can hold several statements",
        "switch 1 { case 1: print(\"a\"); print(\"b\"); case 2: print(\"c\"); }",
        "a\nb\nc\n"
    );

    expectOutput(
        "a switch inside a loop keeps the loop counters apart",
        "let out = []; for (let n in [1, 2]) { switch n { case 1: continue; case 2: break; } out.push(n); } print(out.join(\",\"));",
        "2\n"
    );

    expectOutput(
        "a switch closes a captured value",
        "let fs = []; for (let n in [1, 2]) { switch n { case 1: case 2: fs.push(function() { return n; }); } } print(fs[0]() + fs[1]());",
        "3\n"
    );

    expectRuntimeError(
        "a switch without a case is a compile error",
        "switch 1 { }",
        "at least one case"
    );

    expectRuntimeError(
        "break outside a loop or a switch is refused",
        "break;",
        "only allowed inside a loop or a switch"
    );

    expectErrorText(
        "two defaults are refused",
        "break;",
        "only allowed inside a loop or a switch"
    );

    expectRuntimeError(
        "two defaults are a compile error",
        "switch 1 { default: default: }",
        "already has a default"
    );
}

void classTests() {
    expectOutput(
        "a class makes instances",
        "class P { constructor(x, y) { this.x = x; this.y = y; } }"
        "let p = new P(1, 2); print(p.x, p.y);",
        "1 2\n"
    );

    expectOutput(
        "a method reads this",
        "class P { constructor(x) { this.x = x; } get() { return this.x; } }"
        "print(new P(7).get());",
        "7\n"
    );

    expectOutput(
        "a constructor returns the instance",
        "class P { constructor() {} } print(type(new P()));",
        "P\n"
    );

    expectOutput(
        "fields get class defaults",
        "class P { size = 3; } print(new P().size);",
        "3\n"
    );

    expectOutput(
        "methods can chain",
        "class P { constructor() { this.n = 0; }"
        " bump() { this.n = this.n + 1; return this; } }"
        "print(new P().bump().bump().n);",
        "2\n"
    );

    expectOutput(
        "super runs the base constructor",
        "class A { constructor(a) { this.a = a; } }"
        "class B extends A { constructor(a, b) { super(a); this.b = b; } }"
        "let x = new B(1, 2); print(x.a, x.b);",
        "1 2\n"
    );

    expectOutput(
        "super reaches a base method",
        "class A { m() { return 5; } }"
        "class B extends A { m() { return super.m() + 1; } }"
        "print(new B().m());",
        "6\n"
    );

    expectOutput(
        "a subclass overrides a method",
        "class A { m() { return \"a\"; } } "
        "class B extends A { m() { return \"b\"; } }"
        "print(new B().m());",
        "b\n"
    );

    expectOutput(
        "a static method runs on the class",
        "class C { static make() { return 5; } } print(C.make());",
        "5\n"
    );

    expectOutput(
        "toString formats an instance",
        "class P { toString() { return \"P!\"; } } print(new P());",
        "P!\n"
    );

    expectOutput(
        "toString is inherited",
        "class A { tag = 1; toString() { return str(this.tag); } } "
        "class B extends A {} "
        "print(new B());",
        "1\n"
    );

    expectOutput(
        "a base field survives a subclass",
        "class A { constructor(a) { this.a = a; } }"
        "class B extends A { constructor() { super(4); } }"
        "print(new B().a);",
        "4\n"
    );

    expectRuntimeError(
        "new rejects a plain function",
        "let f = num(1); new f;",
        "needs a class"
    );

    expectRuntimeError(
        "super needs a base class",
        "class A { m() { return super.m(); } } new A().m();",
        "has no base class"
    );
}

void errorTests() {
    expectOutput(
        "catch runs on a throw",
        "try { throw 1; } catch e { print(e); }",
        "1\n"
    );

    expectOutput(
        "the try body stops at a throw",
        "try { print(1); throw 2; print(3); } catch e { print(e); }",
        "1\n2\n"
    );

    expectOutput(
        "catch is skipped without a throw",
        "try { print(1); } catch e { print(2); }",
        "1\n"
    );

    expectOutput(
        "catch can be written without parens",
        "try { throw \"x\"; } catch e { print(e); }",
        "x\n"
    );

    expectOutput(
        "catch can be written with parens",
        "try { throw \"x\"; } catch(e) { print(e); }",
        "x\n"
    );

    expectOutput(
        "finally runs after the body",
        "try { print(1); } finally { print(2); }",
        "1\n2\n"
    );

    expectOutput(
        "finally runs after a catch",
        "try { throw 1; } catch e { print(e); } finally { print(2); }",
        "1\n2\n"
    );

    expectOutput(
        "a throw crosses a function call",
        "function f() { throw 7; }"
        "try { f(); } catch e { print(e); }",
        "7\n"
    );

    expectOutput(
        "a throw unwinds through finally",
        "function f() { try { throw 1; } finally { print(\"clean\"); } }"
        "try { f(); } catch e { print(e); }",
        "clean\n1\n"
    );

    expectOutput(
        "finally runs before an outer catch",
        "try { try { throw 1; } finally { print(2); } } catch e { print(e); }",
        "2\n1\n"
    );

    expectOutput(
        "nested try blocks pick the inner catch",
        "try { try { throw 1; } catch e { print(e); } } catch e { print(2); }",
        "1\n"
    );

    expectOutput(
        "an outer catch handles a rethrow",
        "try {"
        "  try { throw 1; } catch e { throw e; }"
        "} catch e { print(e); }",
        "1\n"
    );
}

void destructureTests() {
    expectOutput(
        "a list pattern binds each name",
        "let [a, b] = [1, 2]; print(a, b);",
        "1 2\n"
    );

    expectOutput(
        "a short list leaves nil behind",
        "let [a, b, c] = [1]; print(a, b, c);",
        "1 nil nil\n"
    );

    expectOutput(
        "a map pattern binds by key",
        "let {x, y} = {x: 1, y: 2}; print(x, y);",
        "1 2\n"
    );

    expectOutput(
        "a missing key leaves nil behind",
        "let {x, gone} = {x: 1}; print(x, gone);",
        "1 nil\n"
    );

    expectOutput(
        "a renamed key reads the old name",
        "let {x: alias} = {x: 5}; print(alias);",
        "5\n"
    );

    expectOutput(
        "a string key works in a pattern",
        "let {\"a key\": v} = {\"a key\": 9}; print(v);",
        "9\n"
    );

    expectOutput(
        "a rest pattern collects the tail",
        "let [a, ...rest] = [1, 2, 3, 4]; print(a, rest);",
        "1 [2, 3, 4]\n"
    );

    expectOutput(
        "an empty rest collects nothing",
        "let [a, ...rest] = [1]; print(a, rest);",
        "1 []\n"
    );

    expectOutput(
        "a nested list pattern digs in",
        "let [[a, b], [c]] = [[1, 2], [3]]; print(a, b, c);",
        "1 2 3\n"
    );

    expectOutput(
        "a nested map pattern digs in",
        "let {a: {b}} = {a: {b: 7}}; print(b);",
        "7\n"
    );

    expectOutput(
        "a mixed pattern reads both sides",
        "let {list: [a, b]} = {list: [1, 2]}; print(a, b);",
        "1 2\n"
    );

    expectOutput(
        "a string iterates by character",
        "let [x, y, z] = \"ab\"; print(x, y, z);",
        "a b nil\n"
    );

    expectOutput(
        "a pattern works inside a function",
        "function f() { let [a, b] = [3, 4]; return a + b; } print(f());",
        "7\n"
    );

    expectOutput(
        "a pattern works inside a loop",
        "let total = 0;"
        "for (let pair in [[1, 2], [3, 4]]) {"
        "  let [a, b] = pair; total = total + a + b;"
        "}"
        "print(total);",
        "10\n"
    );

    expectOutput(
        "a pattern closes over its names",
        "function f() { let {a, b} = {a: 2, b: 3}; return function() { return a * b; }; }"
        "print(f()());",
        "6\n"
    );

    expectOutput(
        "a pattern at the top level stays visible",
        "let [a, b] = [1, 2]; function f() { return a + b; } print(f());",
        "3\n"
    );

    expectOutput(
        "a pattern shadows an outer name",
        "let a = 100; if true { let [a] = [1]; } print(a);",
        "100\n"
    );

    expectOutput(
        "a rest list is a new list",
        "let [a, ...rest] = [1, 2, 3]; rest.push(4); print(a, rest);",
        "1 [2, 3, 4]\n"
    );

    expectOutput(
        "a skipped slot keeps its position",
        "let [, second, , fourth] = [1, 2, 3, 4]; print(second, fourth);",
        "2 4\n"
    );

    expectOutput(
        "an underscore takes no name",
        "let {a: _, b} = {a: 1, b: 2}; print(b);",
        "2\n"
    );

    expectRuntimeError(
        "a number cannot feed a list pattern",
        "let [a] = 5;",
        "cannot iterate over a value of type number"
    );
}

void errorTraceTests() {
    checks++;

    VM vm;
    NovaStd::registerBuiltins(vm);

    Parser parser(Lexer(
        "function inner() { return missingName; }\n"
        "function outer() { return inner(); }\n"
        "outer();"
    ).tokenize());

    Compiler compiler;

    std::string protoName;

    try {
        vm.interpret(compiler.compile(parser.parse(), "test"));
    } catch (const RuntimeError&) {
        protoName = vm.stackTrace();
    }

    std::string shape;

    for (const auto& frame : protoName) {
        shape += (frame == '\n' ? '|' : frame);
    }

    bool good =
        shape.find("in inner at line 1|") != std::string::npos &&
        shape.find("in outer at line 2|") != std::string::npos &&
        shape.find("in test at line 3|") != std::string::npos;

    if (!good) {
        failures++;
        std::cout << "FAILED the trace names every frame: [" << shape
                  << "]\n";
    }

    checks++;
    expectTrue(
        "a trace reads from the inside out",
        protoName.find("inner") < protoName.find("outer") &&
            protoName.find("outer") < protoName.find("test"),
        "the frames are in the wrong order"
    );
}

} 

int main() {
    valueTests();
    stackTests();
    upvalueTests();
    callTests();
    valueSharingTests();
    interpolationTests();
    switchTests();
    classTests();
    errorTests();
    destructureTests();
    errorTraceTests();

    if (failures == 0) {
        std::cout << "all " << checks << " vm checks passed\n";
        return 0;
    }

    std::cout << failures << " vm checks failed\n";
    return 1;
}