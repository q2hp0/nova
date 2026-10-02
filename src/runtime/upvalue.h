#pragma once

#include "value.h"

class Upvalue {
public:
    explicit Upvalue(Value* slot);

    Value get() const;
    void set(const Value& value);
    void close();

    bool isOpen() const;
    Value* slot() const;

private:
    Value* location;
    Value closedValue;
    bool open;
};
