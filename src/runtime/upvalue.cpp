#include "upvalue.h"

Upvalue::Upvalue(Value* slot)
    : location(slot), open(true) {
}

Value Upvalue::get() const {
    if (open) {
        return *location;
    }

    return closedValue;
}

void Upvalue::set(const Value& value) {
    if (open) {
        *location = value;
        return;
    }

    closedValue = value;
}

void Upvalue::close() {
    if (open) {
        closedValue = *location;
        open = false;
        location = nullptr;
    }
}

bool Upvalue::isOpen() const {
    return open;
}

Value* Upvalue::slot() const {
    return location;
}
