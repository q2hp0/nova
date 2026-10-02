#pragma once

#include "object.h"
#include "object_function.h"
#include "value.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ClassObject;

class InstanceObject : public Object {
public:
    explicit InstanceObject(
        std::shared_ptr<ClassObject> definition
    );

    const std::shared_ptr<ClassObject>& definition() const;

    bool has(const std::string& name) const;
    Value get(const std::string& name) const;
    void set(const std::string& name, const Value& value);

    std::vector<std::string> fields() const;
    size_t fieldCount() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::shared_ptr<ClassObject> classDefinition;
    std::vector<std::pair<std::string, Value>> slots;
    std::unordered_map<std::string, size_t> position;
};

class ClassObject : public Object {
public:
    explicit ClassObject(
        std::string name,
        std::shared_ptr<ClassObject> parent = nullptr
    );

    const std::string& name() const;
    const std::shared_ptr<ClassObject>& parent() const;

    bool inheritsFrom(const ClassObject* other) const;

    void defineMethod(
        const std::string& name,
        const Value& value
    );

    Value method(const std::string& name) const;

    void defineField(
        const std::string& name,
        const Value& value
    );

    Value field(const std::string& name) const;

    void defineStatic(
        const std::string& name,
        const Value& value
    );

    Value staticValue(const std::string& name) const;

    bool hasStatic(const std::string& name) const;

    bool isInstantiable() const;
    bool hasOwnConstructor() const;
    Value constructor() const;

    int arity() const;

    std::vector<std::string> fieldOrder() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::string className;
    std::shared_ptr<ClassObject> base;

    std::vector<std::string> methodOrder;
    std::unordered_map<std::string, Value> methods;

    std::vector<std::string> declaredFields;
    std::unordered_map<std::string, Value> defaultFields;

    std::vector<std::string> staticOrder;
    std::unordered_map<std::string, Value> statics;
};