#include "object_class.h"

#include <algorithm>

InstanceObject::InstanceObject(
    std::shared_ptr<ClassObject> definition
)
    : Object(ObjectType::Instance),
      classDefinition(std::move(definition)) {
}

const std::shared_ptr<ClassObject>& InstanceObject::definition() const {
    return classDefinition;
}

bool InstanceObject::has(const std::string& name) const {
    auto found = position.find(name);

    return found != position.end();
}

Value InstanceObject::get(const std::string& name) const {
    auto found = position.find(name);

    if (found == position.end()) {
        if (classDefinition) {
            return classDefinition->field(name);
        }

        return Value();
    }

    return slots[found->second].second;
}

void InstanceObject::set(const std::string& name, const Value& value) {
    auto found = position.find(name);

    if (found != position.end()) {
        slots[found->second].second = value;
        return;
    }

    position[name] = slots.size();
    slots.emplace_back(name, value);
}

std::vector<std::string> InstanceObject::fields() const {
    std::vector<std::string> names;

    if (classDefinition) {
        names = classDefinition->fieldOrder();
    }

    for (const auto& slot : slots) {
        if (
            std::find(names.begin(), names.end(), slot.first) ==
            names.end()
        ) {
            names.push_back(slot.first);
        }
    }

    return names;
}

size_t InstanceObject::fieldCount() const {
    return fields().size();
}

std::string InstanceObject::toString() const {
    return "<instance " +
        (classDefinition ? classDefinition->name() : std::string("?")) +
        ">";
}

std::string InstanceObject::inspect() const {
    std::string out = "<instance " +
        (classDefinition ? classDefinition->name() : std::string("?")) +
        " {";

    bool first = true;

    for (const std::string& name : fields()) {
        if (!first) {
            out += ", ";
        }

        first = false;
        out += name + ": " + get(name).inspect();
    }

    return out + "}>";
}

std::string InstanceObject::typeName() const {
    return classDefinition ? classDefinition->name() : "instance";
}

ClassObject::ClassObject(
    std::string name,
    std::shared_ptr<ClassObject> parent
)
    : Object(ObjectType::Class),
      className(std::move(name)),
      base(std::move(parent)) {
}

const std::string& ClassObject::name() const {
    return className;
}

const std::shared_ptr<ClassObject>& ClassObject::parent() const {
    return base;
}

bool ClassObject::inheritsFrom(const ClassObject* other) const {
    const ClassObject* walk = this;

    while (walk != nullptr) {
        if (walk == other) {
            return true;
        }

        walk = walk->base.get();
    }

    return false;
}

void ClassObject::defineMethod(
    const std::string& name,
    const Value& value
) {
    if (methods.find(name) == methods.end()) {
        methodOrder.push_back(name);
    }

    methods[name] = value;
}

Value ClassObject::method(const std::string& name) const {
    const ClassObject* walk = this;

    while (walk != nullptr) {
        auto found = walk->methods.find(name);

        if (found != walk->methods.end()) {
            return found->second;
        }

        walk = walk->base.get();
    }

    return staticValue(name);
}

void ClassObject::defineField(
    const std::string& name,
    const Value& value
) {
    if (defaultFields.find(name) == defaultFields.end()) {
        declaredFields.push_back(name);
    }

    defaultFields[name] = value;
}

Value ClassObject::field(const std::string& name) const {
    const ClassObject* walk = this;

    while (walk != nullptr) {
        auto found = walk->defaultFields.find(name);

        if (found != walk->defaultFields.end()) {
            return found->second;
        }

        walk = walk->base.get();
    }

    return Value();
}

void ClassObject::defineStatic(
    const std::string& name,
    const Value& value
) {
    if (statics.find(name) == statics.end()) {
        staticOrder.push_back(name);
    }

    statics[name] = value;
}

Value ClassObject::staticValue(const std::string& name) const {
    const ClassObject* walk = this;

    while (walk != nullptr) {
        auto found = walk->statics.find(name);

        if (found != walk->statics.end()) {
            return found->second;
        }

        walk = walk->base.get();
    }

    return Value();
}

bool ClassObject::hasStatic(const std::string& name) const {
    const ClassObject* walk = this;

    while (walk != nullptr) {
        if (walk->statics.find(name) != walk->statics.end()) {
            return true;
        }

        walk = walk->base.get();
    }

    return false;
}

std::vector<std::string> ClassObject::fieldOrder() const {
    std::vector<std::string> names;

    if (base) {
        names = base->fieldOrder();
    }

    for (const std::string& name : declaredFields) {
        if (
            std::find(names.begin(), names.end(), name) == names.end()
        ) {
            names.push_back(name);
        }
    }

    return names;
}

bool ClassObject::isInstantiable() const {
    return method("constructor").isObject();
}

int ClassObject::arity() const {
    Value constructor = method("constructor");

    if (constructor.isObject() &&
        constructor.asObject()->type() == ObjectType::Function) {
        return std::static_pointer_cast<FunctionObject>(
            constructor.asObject()
        )->arity();
    }

    return 0;
}

bool ClassObject::hasOwnConstructor() const {
    return methods.find("constructor") != methods.end();
}

Value ClassObject::constructor() const {
    return method("constructor");
}

std::string ClassObject::toString() const {
    return "<class " + className + ">";
}

std::string ClassObject::inspect() const {
    return toString();
}

std::string ClassObject::typeName() const {
    return "class";
}