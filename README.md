# nova

ok so real talk first.

this is a personal toy project. i made it for fun and to actually learn how a
programming language works from the inside: lexing, parsing, compiling to
bytecode, running that bytecode on a little stack vm. it is not a product, it is
not finished, it has sharp edges everywhere, and it is **not** meant to be used
for anything real. do not run your business, your server or your homework on it.
if you want a real scripting language go use python or lua or javascript. this
thing is a learning playground, and the whole point was the journey, not to ship
a tool.

that said it does work, it has tests, and i had a lot of fun building it, so i
wrote everything down here mostly for future me.

nova is a small scripting language written from scratch in c++17. it compiles to
bytecode and runs on a stack vm (no tree walking at runtime). here is the vibe:

```
let name = "nova";

function greet(who) {
    return "hello " + who;
}

print(greet(name));
```

## build

you need a c++17 compiler and cmake 3.16 or newer. that's it, no dependencies.

```
cmake -S . -B build
cmake --build build -j4
```

the build drops a few things in `build/`:

- `nova` the command line interpreter
- `nova_core` a static library a c++ host can link
- `nova_diagnostics` the error path test runner

if you actually want to install it (please don't on a real machine, but you can
on a throwaway vm):

```
cmake --install build --prefix /usr/local
```

## running programs

```
./build/nova run programs/hello.nova     run a program
./build/nova programs/hello.nova         same as run
./build/nova check programs/hello.nova   compile and print the bytecode
./build/nova --bytecode programs/hello.nova   run and also show the bytecode
./build/nova repl                        start the interactive prompt
./build/nova --version                   print the version
```

the prompt keeps one running program alive, so values stay around between lines.
a line that is a single expression prints its value, a declaration prints
nothing, and a line that isn't done yet just waits for more input:

```
>>> let greeting = "hello";
>>> greeting + " world"
hello world
>>> function twice(fn, value) {
...     return fn(fn(value));
... }
>>> twice(function(n) { return n + 3; }, 1)
7
>>> exit
```

errors come with the source line and the call stack, which is honestly the
feature i'm happiest about:

```
nova: list index 10 out of range, size is 3
trace:
  in main at line 7
  in programs/errors.nova at line 10
```

## values

| type | literals | notes |
| --- | --- | --- |
| number | `42`, `4.5`, `1e3` | every number is a 64 bit float |
| string | `"nova"`, `'nova'` | immutable, and an index is a byte |
| boolean | `true`, `false` | `nil` is the only other false thing |
| nil | `nil`, `null` | the missing value |
| list | `[1, 2, 3]` | ordered, mutable, holds anything |
| map | `{"a": 1}` | keeps key order, mutable, keys are string/number/boolean |
| function | `print`, `function() {}` | both named and anonymous |

`bool(0)` is `true`, by the way. only `nil` and `false` are false, so every
number is truthy. i did that on purpose because i find javascript's truthiness
rules annoying.

## variables

```
let name = "nova";
let count = 0;

count = count + 1;
count += 1;
count -= 1;
count *= 2;
count /= 2;
count %= 5;
```

a `let` inside a block lives only in that block:

```
let value = "outer";

{
    let value = "inner";
    print(value);   // inner
}

print(value);       // outer
```

## operators

arithmetic `+ - * / %`, comparison `== != < <= > >=`, logic `and or not`,
compound assignment `+= -= *= /= %=`, indexing `value[index]`, and method calls
`value.name(...)`.

`+` will glue a number onto a string, so `"value = " + 5` gives `"value = 5"`.
lists and maps can be added too, and the right side gets copied in. `/` always
gives a float. dividing by zero, or taking a remainder by zero, is an error.

## control flow

```
let count = 0;

if count > 3 {
    print("many");
} else if count > 1 {
    print("some");
} else {
    print("few");
}

while count < 10 {
    count = count + 1;

    if count == 5 {
        continue;
    }

    if count == 8 {
        break;
    }
}

for (let i = 0; i < 5; i = i + 1) {
    print(i);
}

for (let value in [10, 20, 30]) {
    print(value);
}

for (let letter in "nova") {
    print(letter);
}

switch season {
    case "summer":
        print("hot");
        break;
    case "winter":
        print("cold");
        break;
    default:
        print("mild");
}
```

conditions can be written with or without parentheses, your call. `for (let
value in ...)` walks a list, a map or a string. the loop variable is a fresh
binding each turn, which matters a lot: closures you make inside a loop keep
their own turn's value instead of all pointing at the last one.

a `switch` picks one branch. the value on the left is compared to each `case` in
order with `==`, and the first match runs. like c, a case with no `break` falls
through into the next one, so `case 1:` with nothing under it shares the body of
the case below. `default` runs when nothing matched and can sit anywhere. a case
body is one statement, a brace block, or as many statements as you want until
the next `case`/`default`/`}`. `break` leaves the switch, but `continue` still
belongs to the loop around it. a switch needs at least one case or a default, and
two defaults are an error. `switch true { ... }` with conditions as the cases is
a nice trick once you have more than two branches.

## functions

```
function add(a, b) {
    return a + b;
}

let double = function(n) {
    return n * 2;
};

let numbers = [1, 2, 3].map(double);
print(add(numbers[0], numbers[1]));   // 3
```

functions are first class: shove them in lists and maps, pass them around, return
them. under the hood a closure is a pointer to a `functionproto` (the compiled
chunk) plus a bundle of captured upvalues.

## closures

a function keeps the variables it captured, even after the call that made them is
long gone:

```
function makeCounter(start) {
    let value = start;

    function step(by) {
        value = value + by;
        return value;
    }

    return {step: step, peek: function() { return value; }};
}

let counter = makeCounter(10);
counter.step(5);
print(counter.peek());   // 15
```

the way this works: when the outer function returns, any local that some inner
function grabbed gets moved off the stack onto the heap as an "upvalue" object.
the inner function holds a pointer to that heap cell, not to a copy, so both
`step` and `peek` see the same `value`. when the outer scope is still running the
upvalue points back into the stack slot, and when the scope ends it gets closed
and copied to the heap. that is the classic craft/store-stack trick and it is
probably the part of the vm i'm proudest of.

## classes

a class has fields, a constructor, methods and static members. `new` builds an
instance, `this` is the current object, and `extends` does inheritance.

```
class Animal {
    constructor(name) {
        this.name = name;
    }

    speak() {
        return this.name + " makes a sound";
    }

    toString() {
        return "Animal(" + this.name + ")";
    }
}

class Dog extends Animal {
    constructor(name, good) {
        super(name);
        this.good = good;
    }

    speak() {
        return super.speak() + " - woof";
    }

    static create(name) {
        return new Dog(name, true);
    }
}

let dog = new Dog("Rex", true);

print(dog.name);         // Rex
print(dog.speak());      // Rex makes a sound - woof
print(str(dog));         // Animal(Rex), through toString
print(Dog.create("Ada").good);   // true
```

a class without a constructor just gets an empty one. `super(name)` calls the
parent constructor and `super.method()` calls the parent method. there are no
field declarations in the class body, you just set `this.x` in the constructor.

the object model is pretty simple: a `classobject` holds a name, a parent
pointer and two maps (instance methods and static values). an `instanceobject`
holds a pointer back to its class plus its own field map. when you call a method
the vm walks the parent chain looking for the name. `toString` is special: if an
instance defines it, `print` and `str` call it, so you get nice output for free.

## errors

`throw` raises a value, `try` guards a block, `catch` receives the value, and
`finally` always runs. a throw unwinds the call stack until it finds a matching
`catch` or `finally`.

```
function divide(a, b) {
    if (b == 0) {
        throw "division by zero";
    }

    return a / b;
}

try {
    print(divide(10, 2));
    print(divide(1, 0));
} catch problem {
    print("caught:", problem);
} finally {
    print("cleanup done");
}
```

```
5
caught: division by zero
cleanup done
```

the variable after `catch` can go with or without parentheses, whatever you
prefer. a throw that nobody catches stops the program and prints `uncaught
error` plus a trace. a `finally` runs whether the body finished, threw, or got
caught.

how it actually works: when the compiler sees a `try` it emits a `push_handler`
that pushes a small record onto a handler stack (where to jump, how deep the
value stack was, and so on). `throw` walks that handler stack, truncates the
value stack back to the recorded depth, pops call frames until it reaches the
handler's frame, and jumps to the catch (or to the cleanup block for a
`finally`). the `finally` block sets an "unwinding" flag so that when it ends it
knows to rethrow the saved value instead of just continuing. this took me way
longer than i want to admit and there was a genuinely evil bug where the catch
target came out as `0x40000009` instead of `9` because i masked with
`0x7fffffff` and forgot that the high flag bit sits inside that mask. fun times.

## modules

a program can split into files. `export` marks the names a module shares, and
`import "path" as name;` binds the module.

```
// math.nova
export square, label;

function square(n) {
    return n * n;
}

let label = "math";
```

```
// main.nova
import "math.nova" as math;

print(math.square(6));   // 36
print(math.label);       // math
```

a relative path is resolved against the directory of the file that imports it. a
module runs once: the second `import` of the same path just reuses the first
result. each module gets its own globals so two modules can't stomp on each
other, but they all share the built in functions. reading a name a module does
not export is an error, not `nil`, because silently handing back `nil` hides
typos.

the trick for isolation is that loading a module saves the caller's globals map,
swaps in a fresh one seeded with the builtins, runs the module in a new call
frame pushed on top of the current one (not by clearing the vm), publishes the
exports, then swaps the old globals back. and `export` is deferred: the compiler
just records the names and emits the export code at the very end of the module,
so you can export something before you define it. that last bit is why the
example above works even though `label` is defined after the `export` line.

the `tests/modules/` pair is a runnable example if you want to poke at it.

## lists

```
let numbers = [3, 1, 2];

numbers.push(4);                    // add one item at the end
numbers.insert(0, 9);               // insert at an index
numbers.remove(0);                  // remove by index
numbers.pop();                      // remove and return the last item

let flipped = numbers.reverse();    // returns a new list
let ordered = numbers.sort();       // returns a new list
let copy = numbers.copy();          // shallow copy

numbers[0] = 7;                     // index assignment
```

indices are zero based. reading or writing past the end, or a negative index, is
an error.

`push`, `insert`, `remove` and `pop` change the list in place and hand it back,
so you can chain them. `reverse`, `sort`, `copy`, `slice`, `map` and `filter`
return a new list and leave the original alone. `sort` sorts numbers
numerically, and with no argument it compares other values by their text form.

methods: `len`, `push`, `pop`, `insert`, `remove`, `indexOf`, `contains`, `join`,
`slice`, `reverse`, `at`, `first`, `last`, `map`, `filter`, `reduce`, `sort`,
`each`, `min`, `max`, `copy`.

```
let names = ["ada", "linus"];

print(names.join(", "));
print(names.map(function(n) { return n.upper(); }).join(" "));
print(names.filter(function(n) { return len(n) > 3; }).len());
print(names.reduce(function(all, n) { return all + n; }, ""));
```

`map`, `filter`, `each` and `reduce` walk the list: `map` returns a new list,
`filter` returns the items that passed, `each` returns nil, and `reduce` takes
the starting value as its second argument. `slice(from)` and `slice(from, to)`
copy a chunk, and `to` is not included. `min`/`max` error on an empty list.
`at(index)` gives nil when the index is out of range instead of blowing up.

## destructuring

a `let` that starts with `[` or `{` pulls values apart and binds several names at
once.

```
let [first, second, third] = [1, 2];
print(first, second, third);      // 1 2 nil

let {name: title, count} = {name: "nova", count: 3};
print(title, count);              // nova 3
```

list patterns read positions, map patterns read keys.

```
let [head, ...tail] = [10, 20, 30, 40];
print(head, tail);                // 10 [20, 30, 40]

let [, , last] = [1, 2, 3];
print(last);                      // 3

let {outer: {inner}} = {outer: {inner: 42}};
print(inner);                     // 42

let {items: [a, b]} = {items: [4, 5]};
print(a, b);                      // 4 5
```

patterns nest freely, and every part binds a name in the current scope, so they
work inside blocks, loops and closures.

```
function total(pairs) {
    let sum = 0;

    for (let pair in pairs) {
        let [x, y] = pair;
        sum = sum + x + y;
    }

    return sum;
}

print(total([[1, 2], [3, 4]]));   // 10
```

you can rename with `key: name`, and `_` takes a value without naming it.

```
let {"a key": value} = {"a key": 9};
print(value);                     // 9

let {a: _, b: kept} = {a: 1, b: 2};
print(kept);                      // 2
```

anything the pattern doesn't cover becomes nil: a short list leaves the extra
names nil, a missing map key is nil. the right side can be a list, a string
(splits into characters) or a map. a rest pattern always builds a new list, so
poking it leaves the original alone.

limits: patterns are flat or nested, no defaults, no computed keys, no rest in a
map. every name in one pattern has to be unique. and a pattern always needs a
value after `=`.

## maps

```
let person = {"name": "ada", "age": 36};

person["city"] = "london";   // insert
person.age = 37;              // property syntax reads and writes map keys

print(person["name"]);
print(person.keys().join(", "));
print(person.values().len());
print(person.contains("city"));
print(person.missing);        // nil, a missing key is not an error

person.remove("city");
person.clear();
```

keys have to be a string, number or boolean. keys keep the order you first used
them in, so `keys()` is stable. methods: `len`, `size`, `keys`, `values`, `get`,
`set`, `remove`, `contains`, `has`, `clear`.

## strings

```
let text = "Nova language";

print(len(text));
print(text.upper());
print(text.lower());
print(text[0]);
print(text.charAt(1));
print(text.split(" "));
print(text.contains("lang"));
print(text.startsWith("Nova"));
print(text.endsWith("ge"));
print(text.replace("Nova", "NOVA"));
print(text.indexOf("lang"));
print(text.substring(0, 4));
print(text.trim());
print(text.repeat(2));
print(text.chars().join("-"));
print("42".toNumber() + 1);
print("hi {name}");
```

a string is written with double quotes. `\n`, `\t`, `\r`, `\\`, `\"`, `\'`,
`\0`, `\e`, `\{` and `\}` are the escapes. indexing gives a one character
string, and out of range is an error. `toNumber` turns text into a number and
stops the program if the text isn't a number. `substring(from)` and
`substring(from, to)` cut the string, `to` not included, so `substring(0, 4)` of
`"Nova language"` is `"Nova"`. `chars` gives you a list of characters.

a `{` inside a string opens a hole that holds an expression and `}` closes it, so
`"hi {name}"` builds text from the value of `name`. the expression in a hole is
parsed with the full grammar, which is why `{[1, 2].join("-")}` and
`{ {"k": 1}.k }` both work, and a hole holding a number still ends up as text. a
hole can even hold a nested string, like `"{"hi " + name}"`. write `\{` and `\}`
for literal braces. an empty hole, a `{` with no `}`, and an expression that
can't be parsed are all errors that name the offending string. the interpolation
is handled at parse time (the parser walks the hole with a sub-parser) and the
string chunk gets split into a list of literal pieces and `{}` markers.

## built in functions

| function | meaning |
| --- | --- |
| `print(...)` | print values separated by spaces, `print()` prints an empty line |
| `len(value)` | length of a string, list or map |
| `str(value)` | text form of any value |
| `type(value)` | the type name, like `"number"` |
| `input(prompt)` | read one line from standard input |
| `abs`, `floor`, `ceil`, `round`, `sqrt` | number helpers |
| `pow(base, power)` | power |
| `min(...)`, `max(...)` | smallest or largest number |
| `sum(list)`, `range(...)` | sum of a list, and a list of numbers |
| `random()`, `random(limit)`, `random(from, to)` | random number |
| `assert(condition, message)` | stop with a message when the condition is false |
| `error(message)` | stop with a message |
| `readFile(path)` | whole file as a string |
| `writeFile(path, text)` | write a file, returns true |
| `appendFile(path, text)` | add to the end of a file |
| `fileExists(path)` | true when the file can be opened |
| `removeFile(path)` | delete a file, false when it wasn't there |
| `ping(host, timeoutMs?, port?)` | true when the host answers |
| `scan(prefix, first, last, timeoutMs?, port?, jobs?)` | live addresses in a range |
| `scanDetail(...)` | the same, each item a map with `ip` and `port` |
| `lookup(host)` | addresses of a name |
| `get(url, timeoutMs?)` | body of an `http://` page |

```
print(range(1, 5).join(" "));        // 1 2 3 4
print(range(0, 10, 5).join(" "));     // 0 5 10
print(sum([1, 2, 3]));               // 6
```

## files

```
let path = "/tmp/notes.txt";

writeFile(path, "first line\n");
appendFile(path, "second line\n");

print(len(readFile(path)));              // byte count
print(readFile(path).trim().split("\n"));  // a list of lines

print(fileExists(path));                 // true
print(removeFile(path));                 // true
print(removeFile(path));                 // false, nothing left
```

a path that can't be opened is an error, so the program stops with a clear
message. `removeFile` is the only one that answers false instead.

## network

nova can reach the network by opening a tcp connection. it never sends a ping
packet, so a host that drops icmp but answers on a port still shows up. this
part is honestly the sketchiest and i mostly wrote it to scan my own home
network.

```
print(lookup("example.com").join(", "));
print(ping("example.com", 2000));          // true
print(ping("192.168.0.1", 1000, 22));      // is port 22 open?
```

`scan` walks a range and answers with the live addresses. `jobs` sets how many
hosts are checked at once, so a whole `/24` takes a couple seconds.

```
let hosts = scanDetail("192.168.0.", 1, 254, 400);

print("found " + str(len(hosts)) + " hosts");

for (let host in hosts) {
    print(host.ip + " port " + str(host.port));
}
```

```
found 3 hosts
192.168.0.1 port 80
192.168.0.29 port 445
192.168.0.121 port 22
```

`ping` tries ports 80, 443, 22, 445, 3389 and 8080 unless you give it one. it
bails early when nothing answers, which is what makes a scan fast. `get` reads
one address and returns the body of the answer.

```
let page = get("http://example.com/");

print(len(page));
print(page.contains("<html"));
```

two programs live in `programs/network/` because they need a real network, so
they're left out of the test run:

```
./build/nova run programs/network/scan.nova
./build/nova run programs/network/web.nova
```

## embedding

`nova_core` is a static library, so a c++ host can compile and run nova and add
its own functions. include `src/nova.h`, build the standard library, and define
natives with `VM::defineGlobal`.

```
#include "nova.h"

#include <iostream>
#include <vector>

int main() {
    VM vm;
    NovaStd::registerBuiltins(vm);
    vm.markBuiltins();

    vm.defineGlobal("triple", nova::makeNative(
        "triple", 1, 1,
        [](VM&, std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("triple expects a number");
            }
            return Value(args[0].asNumber() * 3);
        }
    ));

    vm.interpret(nova::compile("print(triple(14));"));

    return 0;
}
```

`vm.markBuiltins()` is important: it snapshots the globals after the builtins
are registered, so that module isolation can seed each module with the builtins
without handing it your host's globals. build it by linking `nova_core`:

```
g++ -std=c++17 -Isrc examples/embed.cpp build/libnova_core.a -o nova_embed
```

`examples/embed.cpp` is the full thing and it runs in the test suite.

## under the hood

because this was a learning project, here's the whole pipeline in one place,
which is the stuff i actually wanted to understand when i started.

1. **lexing.** `src/lexer` walks the text and spits out tokens. numbers, strings
   (with interpolation markers), identifiers, keywords, and every operator.
   strings start to hear about interpolation here: a string literal is stored
   with markers where the holes were.

2. **parsing.** `src/parser` is a hand written recursive descent parser. it turns
   tokens into an ast of statements and expressions. precedence and associativity
   are handled by the shape of the expression parser functions (`assignment` down
   to `primary`). every node carries a line number so errors can point at it.

3. **compiling.** `src/compiler` walks the ast and emits bytecode into a `chunk`
   (a list of instructions plus a constant pool). it also does the scoping: a
   `functionscope` tracks locals, a scope depth, and upvalues. the tricky parts
   are resolving a variable to a local vs an upvalue vs a global, patching jump
   offsets (all offsets are absolute here, not relative, so a jump just stores a
   target index), and collecting loop bodies so `break`/`continue` can be
   patched.

4. **running.** `src/vm` is the stack machine. it has a byte stack for values, a
   stack of call frames (each with an instruction pointer), an open upvalue list,
   and a handler stack for exceptions. the main loop is one big `switch` over the
   opcode. calls push a frame and, for methods, leave the receiver in slot zero so
   `this` is just local 0.

the object model lives in `src/runtime`. `Value` is a tagged union that can hold a
number, a bool, nil, or a `shared_ptr<Object>`. every heap thing (string, list,
map, function, class, instance, module, native) is an `Object` subclass with
`toString`/`inspect`/`typeName`. memory is reference counted through `shared_ptr`,
which means cycles leak. i know, i know. a real language needs a mark-and-sweep
collector and that's the first thing i'd add if i ever come back to this.

the standard library in `src/stdlib` defines all the builtins and the string,
list and map methods, and `src/net`/`src/io` are the only parts that touch the
operating system.

## testing

```
cmake -S . -B build
cmake --build build -j4
cd build && ctest --output-on-failure
```

the suite is a mix of c++ unit tests, tests written in nova itself, and example
programs. none of them needs a network. there are 20 test groups and they all
pass:

- `tests/language.nova` runs inside nova itself and checks values, control flow,
  functions, closures, scoping, lists, maps, strings, classes, errors and more.
  it's currently at 191 checks.
- `tests/lexer_tests.cpp` checks the tokenizer on its own: every token type,
  numbers, escapes, the markers a string hole leaves behind, comments, lines, and
  the errors a bad character or bad escape produces.
- `tests/parser_tests.cpp` checks the shape of the tree: which node each kind of
  statement and expression becomes, that operator order and argument lists come
  out right, that a switch keeps its cases in order, and that a broken program
  fails with a message that names the missing piece.
- `tests/vm_tests.cpp` checks the runtime: values and how they print, the value
  stack, upvalues, calls and argument counts, string holes, switch branches,
  stack traces, classes, try/catch/finally, destructuring, and the rules that
  decide when a copy is a copy.
- `tests/diagnostics.cpp` drives the compiler from c++ and checks the compiler,
  runtime error paths, and the exact text `print` produces for every value type.
- every file in `programs/` is run and compared against its `.out` file, so the
  examples can't drift away from the language.
- `tests/modules/` holds a module and the program that imports it, and compares
  the output with `tests/modules/main.out`.
- `examples/embed.cpp` is built and run, so the embedding api stays working.
- the prompt is fed `tests/repl_input.txt` and its output is compared with
  `tests/repl_output.txt` and `tests/repl_error.txt`.

if you change a program's output on purpose, regenerate its expected file:

```
./build/nova run programs/hello.nova > programs/hello.out
```

## project layout

```
src/lexer      tokens
src/parser     statements, expressions and the ast
src/compiler   scopes, jumps, constants and the bytecode chunk
src/vm         the value stack, call frames and the interpreter loop
src/vm/file_module_loader.h   resolves and reads imported files
src/nova.h     the public header for a c++ host
src/runtime    values and the object types
src/stdlib     built in functions and the methods of strings, lists and maps
src/net        tcp connections, host lookup and a small http client
src/io         reading, writing and removing files
src/debug      the bytecode disassembler
src/main.cpp   the command line interface
examples       the embedding example
programs       example programs, each with its expected output
programs/network   examples that need a real network
tests          the language test, the unit tests, the diagnostics runner,
               the prompt test and the expected output helpers
```

## known limits

this is the "don't trust this thing" section, read it before you get ideas:

- strings only join other strings, so a hole always turns into text first.
  building a number out of text needs `num`.
- `throw` only carries a value. there is no separate error type and no way to
  describe where a value was built.
- only a `throw` is caught. a built in error, like a bad index or an unknown
  variable, stops the program and is not delivered to `catch`.
- modules load by relative path and export whole names. no selective imports
  (`import { a } from "m"`), no default export, no package search path. files are
  read whole and there's no binary mode.
- a class field has to be set in the constructor. fields aren't declared in the
  class body, and there's no `private`, no static field, and no operator
  overloading beyond `toString`.
- `==` and `!=` compare lists and maps by identity, so two lists with the same
  items are still two different values. use `join` or `contains` to look at items.
- `get` speaks plain `http` only. no `https`, no cookies, no headers you can set,
  and no upload.
- `ping` and `scan` use tcp connections instead of icmp packets, so a firewall
  that blocks ping but allows a port gives a different answer.
- objects are reference counted, not garbage collected, and cycles are never
  freed. this is the big one.
- `print` writes numbers the way c++ streams do, so very large or very small
  numbers show up in exponent form.

## license

mit. see `LICENSE`.

and again, for real: this is a personal learning project, not a tool. have fun
reading it, but please don't build anything that matters on top of it.
