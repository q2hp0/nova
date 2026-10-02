#pragma once

#include "compiler/compiler.h"
#include "errors/errors.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/object_native.h"
#include "stdlib/stdlib.h"
#include "vm/file_module_loader.h"
#include "vm/vm.h"

#include <memory>
#include <string>

namespace nova {

inline std::shared_ptr<FunctionProto> compile(
    const std::string& source,
    const std::string& name = "<embed>"
) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    Compiler compiler;

    return compiler.compile(parser.parse(), name);
}

inline Value makeNative(
    const std::string& name,
    int minArgs,
    int maxArgs,
    NativeObject::Handler handler
) {
    return Value(
        std::make_shared<NativeObject>(
            name,
            minArgs,
            maxArgs,
            std::move(handler)
        )
    );
}

}
