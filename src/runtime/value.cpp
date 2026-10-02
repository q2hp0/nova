#include "value.h"
#include "object.h"

#include <cmath>
#include <iomanip>
#include <sstream>

std::string formatNumber(double value) {
    if (
        std::isfinite(value) &&
        value == std::floor(value) &&
        std::abs(value) < 1e15
    ) {
        return std::to_string(
            static_cast<long long>(value)
        );
    }

    std::ostringstream out;
    out << std::setprecision(15) << value;

    return out.str();
}

Value::Value()
    : data(std::monostate{}) {
}

Value::Value(double value)
    : data(value) {
}

Value::Value(int value)
    : data(static_cast<double>(value)) {
}

Value::Value(bool value)
    : data(value) {
}

Value::Value(const std::string& value)
    : data(value) {
}

Value::Value(const char* value)
    : data(std::string(value)) {
}

Value::Value(const std::shared_ptr<Object>& object)
    : data(object) {
}

bool Value::isNil() const {
    return std::holds_alternative<std::monostate>(data);
}

bool Value::isNumber() const {
    return std::holds_alternative<double>(data);
}

bool Value::isBool() const {
    return std::holds_alternative<bool>(data);
}

bool Value::isString() const {
    return std::holds_alternative<std::string>(data);
}

bool Value::isObject() const {
    return std::holds_alternative<std::shared_ptr<Object>>(data);
}

double Value::asNumber() const {
    return std::get<double>(data);
}

bool Value::asBool() const {
    return std::get<bool>(data);
}

const std::string& Value::asString() const {
    return std::get<std::string>(data);
}

const std::shared_ptr<Object>& Value::asObject() const {
    return std::get<std::shared_ptr<Object>>(data);
}

bool Value::isTruthy() const {
    if (isNil()) {
        return false;
    }

    if (isBool()) {
        return asBool();
    }

    return true;
}

bool Value::equals(const Value& other) const {
    if (isNumber() && other.isNumber()) {
        return asNumber() == other.asNumber();
    }

    if (isBool() && other.isBool()) {
        return asBool() == other.asBool();
    }

    if (isString() && other.isString()) {
        return asString() == other.asString();
    }

    if (isNil() && other.isNil()) {
        return true;
    }

    if (isObject() && other.isObject()) {
        return asObject() == other.asObject();
    }

    return false;
}

std::string Value::typeName() const {
    if (isNil()) {
        return "nil";
    }

    if (isNumber()) {
        return "number";
    }

    if (isBool()) {
        return "bool";
    }

    if (isString()) {
        return "string";
    }

    return asObject()->typeName();
}

std::string Value::toString() const {
    if (isString()) {
        return asString();
    }

    if (isObject()) {
        return asObject()->toString();
    }

    if (isNil()) {
        return "nil";
    }

    if (isBool()) {
        return asBool() ? "true" : "false";
    }

    return formatNumber(asNumber());
}

std::string Value::inspect() const {
    if (isObject()) {
        return asObject()->inspect();
    }

    if (isString()) {
        return "\"" + asString() + "\"";
    }

    return toString();
}
