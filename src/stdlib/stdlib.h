#pragma once

#include "../runtime/value.h"

#include <string>

class VM;

namespace NovaStd {
    void registerBuiltins(VM& vm);

    Value getProperty(
        const Value& target,
        const std::string& name,
        int line
    );
}
