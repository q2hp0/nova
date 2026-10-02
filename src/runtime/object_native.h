#pragma once

#include "object.h"
#include "value.h"

#include <functional>
#include <string>
#include <vector>

class VM;

class NativeObject : public Object {
public:
    using Handler = std::function<Value(VM&, std::vector<Value>&)>;

    NativeObject(
        const std::string& name,
        int minArgs,
        int maxArgs,
        Handler handler
    );

    const std::string& name() const;
    bool isMethod() const;
    bool acceptsArity(size_t count) const;

    Value call(VM& vm, std::vector<Value>& args) const;

    void bind(const Value& value);
    bool isBound() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::string nativeName;
    int minimumArgs;
    int maximumArgs;
    Handler body;
    Value self;
    bool bound;
};
