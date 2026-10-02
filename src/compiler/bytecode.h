#pragma once

#include "opcode.h"
#include "../runtime/value.h"

#include <string>
#include <vector>

struct Instruction {
    OpCode op;
    int operand;
    int line;
};

class Chunk {
public:
    explicit Chunk(const std::string& name = "main");

    int addConstant(const Value& value);

    size_t emit(OpCode op, int line);
    size_t emit(OpCode op, int operand, int line);

    void patch(size_t position, int operand);

    size_t count() const;

    const std::string& name() const;
    const std::vector<Instruction>& code() const;
    const std::vector<Value>& constants() const;

private:
    std::string chunkName;
    std::vector<Instruction> instructions;
    std::vector<Value> constantValues;
};
