#include "opcode.h"

const char* opcodeName(OpCode op) {
    switch (op) {
        case OpCode::Constant: return "CONSTANT";
        case OpCode::Nil: return "NIL";
        case OpCode::True: return "TRUE";
        case OpCode::False: return "FALSE";
        case OpCode::Pop: return "POP";

        case OpCode::GetLocal: return "GET_LOCAL";
        case OpCode::SetLocal: return "SET_LOCAL";
        case OpCode::GetUpvalue: return "GET_UPVALUE";
        case OpCode::SetUpvalue: return "SET_UPVALUE";
        case OpCode::CloseUpvalue: return "CLOSE_UPVALUE";

        case OpCode::GetGlobal: return "GET_GLOBAL";
        case OpCode::SetGlobal: return "SET_GLOBAL";

        case OpCode::GetProperty: return "GET_PROPERTY";
        case OpCode::SetProperty: return "SET_PROPERTY";

        case OpCode::Index: return "INDEX";
        case OpCode::SetIndex: return "SET_INDEX";
        case OpCode::BuildList: return "BUILD_LIST";
        case OpCode::BuildMap: return "BUILD_MAP";
        case OpCode::ToList: return "TO_LIST";

        case OpCode::DefineClass: return "DEFINE_CLASS";
        case OpCode::InheritClass: return "INHERIT";
        case OpCode::Method: return "METHOD";
        case OpCode::DefineStatic: return "DEFINE_STATIC";
        case OpCode::BindMethod: return "BIND_METHOD";
        case OpCode::Construct: return "CONSTRUCT";
        case OpCode::Invoke: return "INVOKE";
        case OpCode::SuperInvoke: return "SUPER_INVOKE";
        case OpCode::SuperCall: return "SUPER_CALL";
        case OpCode::This: return "THIS";

        case OpCode::LoadModule: return "LOAD_MODULE";
        case OpCode::Require: return "REQUIRE";
        case OpCode::Export: return "EXPORT";

        case OpCode::Throw: return "THROW";
        case OpCode::Rethrow: return "RETHROW";
        case OpCode::PushHandler: return "PUSH_HANDLER";
        case OpCode::SetCleanup: return "SET_CLEANUP";
        case OpCode::EndFinally: return "END_FINALLY";
        case OpCode::PopHandler: return "POP_HANDLER";

        case OpCode::Add: return "ADD";
        case OpCode::Subtract: return "SUBTRACT";
        case OpCode::Multiply: return "MULTIPLY";
        case OpCode::Divide: return "DIVIDE";
        case OpCode::Modulo: return "MODULO";

        case OpCode::Negate: return "NEGATE";
        case OpCode::Not: return "NOT";

        case OpCode::Equal: return "EQUAL";
        case OpCode::NotEqual: return "NOT_EQUAL";
        case OpCode::Greater: return "GREATER";
        case OpCode::GreaterEqual: return "GREATER_EQUAL";
        case OpCode::Less: return "LESS";
        case OpCode::LessEqual: return "LESS_EQUAL";

        case OpCode::Jump: return "JUMP";
        case OpCode::JumpIfFalse: return "JUMP_IF_FALSE";
        case OpCode::JumpIfTrue: return "JUMP_IF_TRUE";
        case OpCode::Loop: return "LOOP";

        case OpCode::Call: return "CALL";
        case OpCode::Return: return "RETURN";

        case OpCode::Halt: return "HALT";
    }

    return "UNKNOWN";
}
