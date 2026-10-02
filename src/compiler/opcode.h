#pragma once

enum class OpCode {
    Constant,
    Nil,
    True,
    False,
    Pop,

    GetLocal,
    SetLocal,
    GetUpvalue,
    SetUpvalue,
    CloseUpvalue,

    GetGlobal,
    SetGlobal,

    GetProperty,
    SetProperty,

    Index,
    SetIndex,
    BuildList,
    BuildMap,
    ToList,

    DefineClass,
    InheritClass,
    Method,
    DefineStatic,
    BindMethod,
    Construct,
    Invoke,
    SuperInvoke,
    SuperCall,
    This,
    LoadModule,
    Require,
    Export,

    Throw,
    Rethrow,
    PushHandler,
    SetCleanup,
    EndFinally,
    PopHandler,

    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,

    Negate,
    Not,

    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,

    Jump,
    JumpIfFalse,
    JumpIfTrue,
    Loop,

    Call,
    Return,

    Halt
};

const char* opcodeName(OpCode op);
