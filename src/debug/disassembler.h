#pragma once

#include "../compiler/bytecode.h"

#include <string>
#include <vector>

void disassembleChunk(
    const Chunk& chunk,
    const std::string& name
);

void disassembleInstructions(
    const std::vector<Instruction>& code,
    const std::vector<Value>& constants
);
