#pragma once

#include "object.h"
#include "value.h"

#include <string>
#include <unordered_map>
#include <vector>

class ModuleObject : public Object {
public:
    explicit ModuleObject(std::string path);

    const std::string& path() const;

    void exportValue(
        const std::string& name,
        const Value& value
    );

    Value get(const std::string& name) const;
    bool contains(const std::string& name) const;

    std::vector<std::string> names() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::string modulePath;
    std::vector<std::string> exportOrder;
    std::unordered_map<std::string, Value> exported;
};