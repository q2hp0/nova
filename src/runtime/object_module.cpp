#include "object_module.h"

ModuleObject::ModuleObject(std::string path)
    : Object(ObjectType::Module), modulePath(std::move(path)) {
}

const std::string& ModuleObject::path() const {
    return modulePath;
}

void ModuleObject::exportValue(
    const std::string& name,
    const Value& value
) {
    if (exported.find(name) == exported.end()) {
        exportOrder.push_back(name);
    }

    exported[name] = value;
}

Value ModuleObject::get(const std::string& name) const {
    auto found = exported.find(name);

    if (found == exported.end()) {
        return Value();
    }

    return found->second;
}

bool ModuleObject::contains(const std::string& name) const {
    return exported.find(name) != exported.end();
}

std::vector<std::string> ModuleObject::names() const {
    return exportOrder;
}

std::string ModuleObject::toString() const {
    return "<module " + modulePath + ">";
}

std::string ModuleObject::inspect() const {
    std::string out = "<module " + modulePath + " {";

    bool first = true;

    for (const std::string& name : exportOrder) {
        if (!first) {
            out += ", ";
        }

        first = false;
        out += name + ": " + exported.at(name).inspect();
    }

    return out + "}>";
}

std::string ModuleObject::typeName() const {
    return "module";
}