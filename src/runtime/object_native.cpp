#include "object_native.h"

#include "../errors/errors.h"
#include "vm/vm.h"

NativeObject::NativeObject(
    const std::string& name,
    int minArgs,
    int maxArgs,
    Handler handler
)
    : Object(ObjectType::Native),
      nativeName(name),
      minimumArgs(minArgs),
      maximumArgs(maxArgs),
      body(std::move(handler)),
      bound(false) {
}

const std::string& NativeObject::name() const {
    return nativeName;
}

bool NativeObject::isMethod() const {
    return bound;
}

bool NativeObject::acceptsArity(size_t count) const {
    if (static_cast<int>(count) < minimumArgs) {
        return false;
    }

    if (maximumArgs < 0) {
        return true;
    }

    return static_cast<int>(count) <= maximumArgs;
}

Value NativeObject::call(
    VM& vm,
    std::vector<Value>& args
) const {
    if (!acceptsArity(args.size())) {
        throw RuntimeError(
            nativeName +
            " expects " +
            (maximumArgs < 0
                ? "at least " + std::to_string(minimumArgs)
                : std::to_string(minimumArgs) +
                  " to " +
                  std::to_string(maximumArgs)) +
            " arguments, got " +
            std::to_string(args.size())
        );
    }

    return body(vm, args);
}

void NativeObject::bind(const Value& value) {
    self = value;
    bound = true;
}

bool NativeObject::isBound() const {
    return bound;
}

std::string NativeObject::toString() const {
    if (bound) {
        return "<method " + nativeName + ">";
    }

    return "<native " + nativeName + ">";
}

std::string NativeObject::inspect() const {
    return toString();
}

std::string NativeObject::typeName() const {
    return bound ? "method" : "function";
}
