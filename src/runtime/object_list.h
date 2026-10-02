#pragma once

#include "object.h"
#include "value.h"

#include <string>
#include <vector>

class ListObject : public Object {
public:
    explicit ListObject(
        std::vector<Value> elements = {}
    );

    std::vector<Value>& elements();
    const std::vector<Value>& elements() const;

    void push(const Value& value);
    Value pop();
    void insert(size_t index, const Value& value);

    bool remove(const Value& value);
    int find(const Value& value) const;
    bool contains(const Value& value) const;

    Value get(size_t index) const;
    void set(size_t index, const Value& value);

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::vector<Value> items;
};
