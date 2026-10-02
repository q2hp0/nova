#pragma once

#include "../runtime/value.h"

#include <cstddef>
#include <vector>

class ValueStack {
public:
    static const size_t LIMIT = 65536;

    ValueStack();

    void push(const Value& value);
    Value pop();

    Value& top();
    const Value& top() const;

    Value* data();
    Value& at(size_t index);
    const Value& at(size_t index) const;

    void set(size_t index, const Value& value);
    void truncate(size_t size);

    std::vector<Value>::iterator begin();
    std::vector<Value>::iterator end();

    void clear();

    size_t size() const;
    bool empty() const;

private:
    std::vector<Value> values;
};
