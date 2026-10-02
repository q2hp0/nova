# Contributing to NOVA

Thanks for helping. This file explains how to build, test and extend NOVA.

## Build and test

```
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

The build needs a C++17 compiler and CMake 3.16 or newer. There are no third
party dependencies.

## Layout

- `src/lexer` turns text into tokens.
- `src/parser` turns tokens into an AST.
- `src/compiler` walks the AST and emits bytecode.
- `src/vm` runs the bytecode.
- `src/runtime` holds values and object types.
- `src/stdlib` holds built in functions and string, list and map methods.
- `src/net` and `src/io` hold the network and file helpers.
- `src/debug` holds the disassembler.
- `src/nova.h` is the public header for a C++ host.
- `programs/` and `tests/` hold examples and tests.

## Adding a feature

1. Add or change the syntax in the lexer and parser.
2. Give it a node in `src/parser/ast.h`.
3. Compile it in `src/compiler/compiler.cpp`. Add an opcode if you need one.
4. Run it in `src/vm/vm.cpp` if you added an opcode.
5. Add a unit test in `tests/` and an example in `programs/` with its `.out`.

## Tests

Keep the suite green. A change to behavior should come with a test that would
fail before the change:

- `tests/lexer_tests.cpp` for tokens.
- `tests/parser_tests.cpp` for the shape of the tree.
- `tests/vm_tests.cpp` for runtime behavior.
- `tests/diagnostics.cpp` for error messages.
- `programs/*.nova` plus `programs/*.out` for examples.
- `tests/modules/` for module loading.

Regenerate an example output after an intended change:

```
./build/nova run programs/hello.nova > programs/hello.out
```

## Style

- C++17, four space indent, `camelCase` for methods and `PascalCase` for types.
- Keep functions small and put a blank line before `return`.
- Match the surrounding code. Do not add comments unless the code cannot speak
  for itself.
- Errors should name the line and the value that was wrong.
