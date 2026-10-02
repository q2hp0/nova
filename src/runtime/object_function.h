#pragma once

#include "object.h"
#include "../compiler/bytecode.h"

#include <memory>
#include <string>
#include <vector>

class Upvalue;

struct UpvalueSlot {
    bool isLocal;
    int index;
};

class FunctionProto : public Object {
public:
    FunctionProto(
        const std::string& name,
        int arity,
        int slotCount
    );

    const std::string& name() const;
    int arity() const;
    int slotCount() const;
    void setSlotCount(int count);

    void setMethod(bool value);
    bool isMethod() const;

    int addUpvalue(bool isLocal, int index);
    const std::vector<UpvalueSlot>& upvalues() const;

    Chunk& chunk();
    const Chunk& chunk() const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::string functionName;
    int parameterCount;
    int localCount;
    bool methodFlag = false;
    std::vector<UpvalueSlot> upvalueSlots;
    Chunk functionChunk;
};

class FunctionObject : public Object {
public:
    explicit FunctionObject(
        std::shared_ptr<FunctionProto> proto
    );

    const std::shared_ptr<FunctionProto>& proto() const;
    const std::string& name() const;
    int arity() const;


    void bindUpvalue(
        int slot,
        std::shared_ptr<Upvalue> upvalue
    );

    std::shared_ptr<Upvalue> upvalue(int slot) const;

    std::string toString() const override;
    std::string inspect() const override;
    std::string typeName() const override;

private:
    std::shared_ptr<FunctionProto> functionProto;
    std::vector<std::shared_ptr<Upvalue>> capturedUpvalues;
};
