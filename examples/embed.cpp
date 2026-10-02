#include "nova.h"

#include <iostream>
#include <vector>

int main() {
    VM vm;
    NovaStd::registerBuiltins(vm);
    vm.markBuiltins();

    vm.defineGlobal("triple", nova::makeNative(
        "triple",
        1,
        1,
        [](VM&, std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("triple expects a number");
            }

            return Value(args[0].asNumber() * 3);
        }
    ));

    const std::string source =
        "let n = triple(14);\n"
        "print(\"n is\", n);\n"
        "print(triple(n));\n";

    std::shared_ptr<FunctionProto> proto = nova::compile(source, "embed");

    vm.interpret(proto);

    return 0;
}
