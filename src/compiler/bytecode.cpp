#include "bytecode.h"

Chunk::Chunk(const std::string& name)
    : chunkName(name) {
}

int Chunk::addConstant(const Value& value) {
    for (size_t i = 0; i < constantValues.size(); ++i) {
        if (constantValues[i].equals(value)) {
            return static_cast<int>(i);
        }
    }

    constantValues.push_back(value);

    return static_cast<int>(constantValues.size() - 1);
}

size_t Chunk::emit(OpCode op, int line) {
    return emit(op, 0, line);
}

size_t Chunk::emit(OpCode op, int operand, int line) {
    instructions.push_back({
        op,
        operand,
        line
    });

    return instructions.size() - 1;
}

void Chunk::patch(size_t position, int operand) {
    instructions[position].operand = operand;
}


size_t Chunk::count() const {
    return instructions.size();
}

const std::string& Chunk::name() const {
    return chunkName;
}

const std::vector<Instruction>& Chunk::code() const {
    return instructions;
}

const std::vector<Value>& Chunk::constants() const {
    return constantValues;
}
