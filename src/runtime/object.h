#pragma once

#include <string>

enum class ObjectType {
    Function,
    Native,
    List,
    Map,
    Class,
    Instance,
    Module
};

class Object {
public:
    explicit Object(ObjectType type);
    virtual ~Object() = default;

    ObjectType type() const;

    virtual std::string toString() const = 0;
    virtual std::string inspect() const = 0;
    virtual std::string typeName() const = 0;

private:
    ObjectType objectType;
};
