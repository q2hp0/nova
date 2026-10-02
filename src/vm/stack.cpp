#include "stack.h"

#include "../errors/errors.h"

ValueStack::ValueStack() {
    values.reserve(LIMIT);
}

void ValueStack::push(const Value& value) {
    if (values.size() >= LIMIT) {
        throw RuntimeError(
            "stack overflow, too much recursion"
        );
    }

    values.push_back(value);
}

Value ValueStack::pop() {
    if (values.empty()) {
        throw RuntimeError("stack underflow");
    }

    Value value = values.back();
    values.pop_back();

    return value;
}

Value& ValueStack::top() {
    if (values.empty()) {
        throw RuntimeError("stack is empty");
    }

    return values.back();
}

const Value& ValueStack::top() const {
    if (values.empty()) {
        throw RuntimeError("stack is empty");
    }

    return values.back();
}

Value* ValueStack::data() {
    return values.data();
}

Value& ValueStack::at(size_t index) {
    if (index >= values.size()) {
        throw RuntimeError("stack access out of range");
    }

    return values[index];
}

const Value& ValueStack::at(size_t index) const {
    if (index >= values.size()) {
        throw RuntimeError("stack access out of range");
    }

    return values[index];
}

void ValueStack::set(
    size_t index,
    const Value& value
) {
    at(index) = value;
}

void ValueStack::truncate(size_t size) {
    if (size > values.size()) {
        throw RuntimeError("stack truncate out of range");
    }

    values.resize(size);
}

std::vector<Value>::iterator ValueStack::begin() {
    return values.begin();
}

std::vector<Value>::iterator ValueStack::end() {
    return values.end();
}

void ValueStack::clear() {
    values.clear();
}

size_t ValueStack::size() const {
    return values.size();
}

bool ValueStack::empty() const {
    return values.empty();
}
