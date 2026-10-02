#include "compiler/compiler.h"
#include "debug/disassembler.h"
#include "errors/errors.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "stdlib/stdlib.h"
#include "vm/file_module_loader.h"
#include "vm/vm.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);

    if (!file) {
        throw NovaError("cannot open file '" + path + "'");
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

std::shared_ptr<FunctionProto> build(
    const std::string& source,
    const std::string& name
) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());

    Compiler compiler;

    return compiler.compile(parser.parse(), name);
}

int reportThrow(
    const NovaThrow& thrown,
    VM* vm
) {
    std::cerr
        << "nova: uncaught error: "
        << thrown.value().toString()
        << '\n';

    if (vm != nullptr) {
        std::string trace = vm->stackTrace();

        if (!trace.empty()) {
            std::cerr << "trace:" << '\n' << trace;
        }
    }

    return 1;
}

int reportFailure(
    const NovaError& error,
    VM* vm
) {
    std::cerr
        << "nova: "
        << error.what()
        << '\n';

    if (vm != nullptr) {
        std::string trace = vm->stackTrace();

        if (!trace.empty()) {
            std::cerr << "trace:" << '\n' << trace;
        }
    }

    return 1;
}

std::string directoryOf(const std::string& path) {
    const size_t slash = path.find_last_of('/');

    if (slash == std::string::npos) {
        return ".";
    }

    if (slash == 0) {
        return "/";
    }

    return path.substr(0, slash);
}

int runProgram(
    const std::string& source,
    const std::string& name,
    bool showBytecode
) {
    std::shared_ptr<FunctionProto> proto = build(source, name);

    if (showBytecode) {
        disassembleChunk(proto->chunk(), name);
    }

    VM vm;
    NovaStd::registerBuiltins(vm);
    vm.markBuiltins();

    FileModuleLoader modules(directoryOf(name));
    vm.setModuleLoader(&modules);

    try {
        vm.interpret(proto);
    } catch (const NovaError& error) {
        return reportFailure(error, &vm);
    } catch (const NovaThrow& thrown) {
        return reportThrow(thrown, &vm);
    }

    return 0;
}

int checkProgram(const std::string& source, const std::string& name) {
    std::shared_ptr<FunctionProto> proto = build(source, name);

    disassembleChunk(proto->chunk(), name);

    std::cout
        << "ok: "
        << name
        << " compiles into "
        << proto->chunk().count()
        << " instructions\n";

    return 0;
}

bool readyToRun(const std::string& source) {
    int depth = 0;
    char quote = 0;
    bool inString = false;
    bool inComment = false;

    for (size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];

        if (inComment) {
            inComment = c != '\n';
            continue;
        }

        if (inString) {
            if (c == '\\') {
                ++i;
            } else if (c == quote) {
                inString = false;
            }

            continue;
        }

        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            inComment = true;
            continue;
        }

        if (c == '"' || c == '\'') {
            inString = true;
            quote = c;
            continue;
        }

        if (c == '{' || c == '(' || c == '[') {
            depth++;
        }

        if (c == '}' || c == ')' || c == ']') {
            depth--;
        }
    }

    return depth <= 0;
}

bool needsMoreInput(const NovaError& error) {
    return std::string(error.what()).find("end of file") !=
        std::string::npos;
}

bool showsItsValue(const std::string& source) {
    try {
        Parser parser(Lexer(source).tokenize());

        return parser.holdsOnlyAnExpression();
    } catch (const NovaError&) {
        return false;
    }
}

int startRepl() {
    VM vm;
    NovaStd::registerBuiltins(vm);
    vm.markBuiltins();

    FileModuleLoader modules(".");
    vm.setModuleLoader(&modules);

    std::string pending;

    std::cout
        << "nova " NOVA_VERSION
        << " | 'exit' to leave"
        << '\n';

    while (true) {
        std::cout
            << (pending.empty() ? ">>> " : "... ")
            << std::flush;

        std::string line;

        if (!std::getline(std::cin, line)) {
            std::cout << '\n';
            break;
        }

        if (pending.empty() &&
            (line == "exit" || line == "quit")) {
            break;
        }

        pending += line + "\n";

        if (!readyToRun(pending)) {
            continue;
        }

        const std::string source = pending;

        std::string program = showsItsValue(source) ?
            "print(" + source + ");" : source;

        pending.clear();

        std::shared_ptr<FunctionProto> proto;

        try {
            proto = build(program, "repl");
        } catch (const ParserError& error) {
            if (needsMoreInput(error)) {
                pending = source;
                continue;
            }

            reportFailure(error, &vm);
            continue;
        } catch (const NovaError& error) {
            reportFailure(error, &vm);
            continue;
        }

        try {
            vm.interpret(proto);
        } catch (const NovaError& error) {
            reportFailure(error, &vm);
        }
    }

    return 0;
}

int runFile(
    const std::string& path,
    bool showBytecode
) {
    return runProgram(
        readFile(path),
        path,
        showBytecode
    );
}

void printUsage() {
    std::cout
        << "usage\n"
        << "  nova run <file.nova>     run a program\n"
        << "  nova check <file.nova>   compile and print the bytecode\n"
        << "  nova repl                start the interactive prompt\n"
        << "  nova <file.nova>         same as run\n"
        << "  nova --bytecode <file>   run and show the bytecode\n"
        << "  nova --version           print the version\n";
}

}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    bool showBytecode = false;
    std::string command;
    std::string path;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--bytecode") {
            showBytecode = true;
            continue;
        }

        if (arg == "--version") {
            std::cout << "nova " NOVA_VERSION "\n";
            return 0;
        }

        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }

        if (command.empty()) {
            command = arg;
            continue;
        }

        path = arg;
    }

    try {
        if (command.empty()) {
            return startRepl();
        }

        if (command == "repl") {
            return startRepl();
        }

        if (path.empty()) {
            std::cerr << "nova: expected a file name\n";
            printUsage();
            return 1;
        }

        if (command == "run") {
            return runFile(path, showBytecode);
        }

        if (command == "check") {
            return checkProgram(readFile(path), path);
        }

        std::cerr << "nova: unknown command '" << command << "'\n";
        printUsage();
        return 1;
    } catch (const NovaError& error) {
        return reportFailure(error, nullptr);
    } catch (const NovaThrow& thrown) {
        return reportThrow(thrown, nullptr);
    } catch (const std::exception& error) {
        std::cerr << "nova: " << error.what() << '\n';
        return 1;
    }
}
