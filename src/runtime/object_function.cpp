#include "object_function.h"
#include "upvalue.h"

#include <stdexcept>

FunctionProto::FunctionProto(
    const std::string& name,
    int arity,
    int slotCount
)
    : Object(ObjectType::Function),
      functionName(name),
      parameterCount(arity),
      localCount(slotCount),
      functionChunk(name) {
}

const std::string& FunctionProto::name() const {
    return functionName;
}

int FunctionProto::arity() const {
    return parameterCount;
}

void FunctionProto::setMethod(bool value) {
    methodFlag = value;
}

bool FunctionProto::isMethod() const {
    return methodFlag;
}

int FunctionProto::slotCount() const {
    return localCount;
}

void FunctionProto::setSlotCount(int count) {
    localCount = count;
}

int FunctionProto::addUpvalue(bool isLocal, int index) {
    for (size_t i = 0; i < upvalueSlots.size(); ++i) {
        if (
            upvalueSlots[i].isLocal == isLocal &&
            upvalueSlots[i].index == index
        ) {
            return static_cast<int>(i);
        }
    }

    upvalueSlots.push_back({
        isLocal,
        index
    });

    return static_cast<int>(upvalueSlots.size() - 1);
}

const std::vector<UpvalueSlot>& FunctionProto::upvalues() const {
    return upvalueSlots;
}

Chunk& FunctionProto::chunk() {
    return functionChunk;
}

const Chunk& FunctionProto::chunk() const {
    return functionChunk;
}

std::string FunctionProto::toString() const {
    return "<function " + functionName + ">";
}

std::string FunctionProto::inspect() const {
    return "<function " + functionName + ">";
}

std::string FunctionProto::typeName() const {
    return "function";
}

FunctionObject::FunctionObject(
    std::shared_ptr<FunctionProto> proto
)
    : Object(ObjectType::Function),
      functionProto(std::move(proto)),
      capturedUpvalues(
          functionProto->upvalues().size()
      ) {
}

const std::shared_ptr<FunctionProto>&
FunctionObject::proto() const {
    return functionProto;
}

const std::string& FunctionObject::name() const {
    return functionProto->name();
}

int FunctionObject::arity() const {
    return functionProto->arity();
}

void FunctionObject::bindUpvalue(
    int slot,
    std::shared_ptr<Upvalue> upvalue
) {
    if (
        slot < 0 ||
        static_cast<size_t>(slot) >=
            capturedUpvalues.size()
    ) {
        throw std::runtime_error(
            "upvalue slot out of range"
        );
    }

    capturedUpvalues[slot] = std::move(upvalue);
}

std::shared_ptr<Upvalue> FunctionObject::upvalue(
    int slot
) const {
    if (
        slot < 0 ||
        static_cast<size_t>(slot) >=
            capturedUpvalues.size()
    ) {
        throw std::runtime_error(
            "upvalue slot out of range"
        );
    }

    return capturedUpvalues[slot];
}

std::string FunctionObject::toString() const {
    return "<function " + name() + ">";
}

std::string FunctionObject::inspect() const {
    return "<function " + name() + ">";
}

std::string FunctionObject::typeName() const {
    return "function";
}
