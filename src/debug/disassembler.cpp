#include "disassembler.h"

#include "../runtime/object_function.h"

#include <iomanip>
#include <iostream>

namespace {

bool readsConstant(OpCode op) {
    return op == OpCode::Constant ||
        op == OpCode::GetGlobal ||
        op == OpCode::SetGlobal ||
        op == OpCode::GetProperty ||
        op == OpCode::SetProperty ||
        op == OpCode::DefineClass ||
        op == OpCode::InheritClass ||
        op == OpCode::Method ||
        op == OpCode::DefineStatic ||
        op == OpCode::BindMethod ||
        op == OpCode::SuperCall ||
        op == OpCode::Construct ||
        op == OpCode::This ||
        op == OpCode::LoadModule ||
        op == OpCode::Require ||
        op == OpCode::Export;
}

bool isJump(OpCode op) {
    return op == OpCode::Jump ||
        op == OpCode::JumpIfFalse ||
        op == OpCode::JumpIfTrue ||
        op == OpCode::Loop;
}

bool readsOperand(OpCode op) {
    return readsConstant(op) ||
        isJump(op) ||
        op == OpCode::Call ||
        op == OpCode::BuildList ||
        op == OpCode::BuildMap ||
        op == OpCode::GetLocal ||
        op == OpCode::SetLocal ||
        op == OpCode::GetUpvalue ||
        op == OpCode::SetUpvalue ||
        op == OpCode::CloseUpvalue ||
        op == OpCode::PopHandler ||
        op == OpCode::Throw;
}

}

void disassembleInstructions(
    const std::vector<Instruction>& code,
    const std::vector<Value>& constants
) {
    for (size_t i = 0; i < code.size(); ++i) {
        const Instruction& instruction = code[i];

        std::cout
            << std::setw(4)
            << i
            << "  "
            << std::setw(16)
            << std::left
            << opcodeName(instruction.op);

        if (readsOperand(instruction.op)) {
            std::cout << instruction.operand;
        }

        if (isJump(instruction.op) &&
            instruction.op != OpCode::Loop &&
            instruction.operand > 0) {
            std::cout << "   -> " << instruction.operand;
        }

        if (readsConstant(instruction.op) &&
            instruction.operand >= 0 &&
            static_cast<size_t>(instruction.operand) < constants.size()) {
            std::cout
                << "   ["
                << constants[instruction.operand].inspect()
                << "]";
        }

        if (instruction.op == OpCode::Invoke ||
            instruction.op == OpCode::SuperInvoke) {
            const int nameIndex = instruction.operand >> 8;
            const int argCount = instruction.operand & 0xFF;

            std::cout << "   " << argCount << " arg";

            if (argCount != 1) {
                std::cout << 's';
            }

            if (nameIndex >= 0 &&
                static_cast<size_t>(nameIndex) < constants.size()) {
                const Value& method = constants[nameIndex];

                if (method.isObject() &&
                    method.asObject()->type() ==
                        ObjectType::Function) {
                    std::cout
                        << " on "
                        << std::static_pointer_cast<FunctionObject>(
                            method.asObject()
                        )->name();
                }
            }
        }

        std::cout
            << "   ; line "
            << instruction.line
            << '\n';
    }
}

void disassembleChunk(
    const Chunk& chunk,
    const std::string& name
) {
    std::cout << "== " << name << " ==\n";

    const std::vector<Instruction> code = chunk.code();
    const std::vector<Value> constants = chunk.constants();

    disassembleInstructions(code, constants);

    std::cout << "constants: " << constants.size() << '\n';

    std::vector<std::shared_ptr<FunctionProto>> nested;

    for (size_t i = 0; i < constants.size(); ++i) {
        const Value& constant = constants[i];

        std::cout
            << "  "
            << i
            << ": "
            << constant.inspect()
            << '\n';

        if (!constant.isObject() ||
            constant.asObject()->type() != ObjectType::Function) {
            continue;
        }

        const std::shared_ptr<Object>& object =
            constant.asObject();

        auto function =
            std::dynamic_pointer_cast<FunctionProto>(object);

        if (function != nullptr) {
            nested.push_back(function);
            continue;
        }

        auto closure =
            std::dynamic_pointer_cast<FunctionObject>(object);

        if (closure != nullptr) {
            nested.push_back(closure->proto());
        }
    }

    for (const std::shared_ptr<FunctionProto>& proto : nested) {
        disassembleChunk(proto->chunk(), proto->name());
    }
}
