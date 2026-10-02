#pragma once

#include "../runtime/object_function.h"

#include <cstddef>
#include <memory>

struct CallFrame {
    std::shared_ptr<FunctionObject> closure;
    std::shared_ptr<FunctionProto> proto;
    size_t ip = 0;
    size_t stackBase = 0;
};
