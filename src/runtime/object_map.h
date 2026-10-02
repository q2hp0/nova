#pragma once

#include "object.h"
#include "value.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class MapObject : public Object {
public:
    explicit MapObject(
        std::vector<std::pair<std::string, Value>> entries = {}
    );

    size_t size() const;
    bool contains(const std::string& key) const;

    Value get(const std::string& key) const;
    void set(const std::string& key, const Value& value);
    bool remove(const std::string& key);
    void clear();

    std::vector<std::string> keys() const;
    std::vector<Value> values() const;

    const std::vector<std::pair<std::string, Value>>&
    entries() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::vector<std::pair<std::string, Value>> items;
    std::unordered_map<std::string, size_t> position;
};

std::string mapKeyOf(const Value& key);
