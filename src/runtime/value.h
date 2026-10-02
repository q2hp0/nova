#pragma once

#include <memory>
#include <string>
#include <variant>

class Object;

using ValueData = std::variant<
    std::monostate,
    double,
    bool,
    std::string,
    std::shared_ptr<Object>
>;

class Value {
public:
    Value();
    explicit Value(double value);
    explicit Value(int value);
    explicit Value(bool value);
    explicit Value(const std::string& value);
    explicit Value(const char* value);
    explicit Value(const std::shared_ptr<Object>& object);

    bool isNil() const;
    bool isNumber() const;
    bool isBool() const;
    bool isString() const;
    bool isObject() const;

    double asNumber() const;
    bool asBool() const;
    const std::string& asString() const;
    const std::shared_ptr<Object>& asObject() const;

    bool isTruthy() const;
    bool equals(const Value& other) const;

    std::string typeName() const;
    std::string toString() const;
    std::string inspect() const;

private:
    ValueData data;
};

std::string formatNumber(double value);
