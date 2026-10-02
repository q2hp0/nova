#include "object.h"

Object::Object(ObjectType type)
    : objectType(type) {
}

ObjectType Object::type() const {
    return objectType;
}
