#include "object_list.h"

#include "../errors/errors.h"

static std::string joinValues(
    const std::vector<Value>& items,
    bool inspect
) {
    std::string out = "[";

    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }

        out += inspect
            ? items[i].inspect()
            : items[i].toString();
    }

    return out + "]";
}

ListObject::ListObject(
    std::vector<Value> elements
)
    : Object(ObjectType::List),
      items(std::move(elements)) {
}

std::vector<Value>& ListObject::elements() {
    return items;
}

const std::vector<Value>& ListObject::elements() const {
    return items;
}

void ListObject::push(const Value& value) {
    items.push_back(value);
}

Value ListObject::pop() {
    if (items.empty()) {
        throw RuntimeError(
            "cannot pop from an empty list"
        );
    }

    Value value = items.back();
    items.pop_back();

    return value;
}

void ListObject::insert(
    size_t index,
    const Value& value
) {
    if (index > items.size()) {
        throw RuntimeError(
            "list index out of range"
        );
    }

    items.insert(
        items.begin() + static_cast<long>(index),
        value
    );
}

int ListObject::find(const Value& value) const {
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].equals(value)) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

bool ListObject::contains(const Value& value) const {
    return find(value) != -1;
}

bool ListObject::remove(const Value& value) {
    int index = find(value);

    if (index < 0) {
        return false;
    }

    items.erase(
        items.begin() + index
    );

    return true;
}

Value ListObject::get(size_t index) const {
    if (index >= items.size()) {
        throw RuntimeError(
            "list index " +
            std::to_string(index) +
            " out of range, size is " +
            std::to_string(items.size())
        );
    }

    return items[index];
}

void ListObject::set(
    size_t index,
    const Value& value
) {
    if (index >= items.size()) {
        throw RuntimeError(
            "list index " +
            std::to_string(index) +
            " out of range, size is " +
            std::to_string(items.size())
        );
    }

    items[index] = value;
}

std::string ListObject::toString() const {
    return joinValues(items, false);
}

std::string ListObject::inspect() const {
    return joinValues(items, true);
}

std::string ListObject::typeName() const {
    return "list";
}
