#pragma once

#include "call_frame.h"
#include "stack.h"
#include "../compiler/bytecode.h"
#include "../runtime/object_function.h"
#include "../runtime/object_native.h"
#include "../runtime/object_class.h"
#include "../runtime/object_module.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct Handler {
    size_t frameIndex = 0;
    size_t stackDepth = 0;
    size_t returnIp = 0;
    size_t cleanupIp = 0;
    bool hasCatch = false;
    bool unwinding = false;
    Value pending;
};

class ModuleLoader {
public:
    virtual ~ModuleLoader() = default;
    virtual std::string load(const std::string& path) = 0;
};

class VM {
public:
    VM();

    void defineGlobal(
        const std::string& name,
        const Value& value
    );

    bool hasGlobal(const std::string& name) const;
    Value getGlobal(const std::string& name) const;

    void markBuiltins();

    void interpret(const Chunk& chunk);
    void interpret(const std::shared_ptr<FunctionProto>& proto);

    Value callFunction(
        const Value& callee,
        std::vector<Value>& args
    );

    void exportValue(
        const std::string& name,
        const Value& value
    );

    ModuleLoader* setModuleLoader(ModuleLoader* loader);

    std::string stackTrace() const;

    std::string errorTrace() const;

    ValueStack& stack();

    const std::string& moduleRoot() const;

    void setModuleRoot(const std::string& path);

    Value display(const Value& value, int line = 0) const;

private:
    void execute(size_t stopDepth = 0);
    void run(int argCount, bool receiverOnStack = false);

    std::shared_ptr<Upvalue> captureUpvalue(
        size_t stackIndex
    );

    Value makeClosure(
        const std::shared_ptr<FunctionProto>& proto
    );

    void closeUpvalues(size_t stackIndex);

    Value local(int slot) const;
    void setLocal(int slot, const Value& value);

    const Value& require(int constantIndex, int line) const;

    Value binary(
        OpCode op,
        const Value& left,
        const Value& right,
        int line
    ) const;

    Value indexOf(
        const Value& target,
        const Value& index,
        int line
    ) const;

    Value toList(const Value& value, int line) const;

    Value lookupMethod(
        const Value& target,
        const std::string& name,
        const ClassObject* startAfter,
        int line
    ) const;

    Value loadModule(const std::string& path, int line);

    Value getPropertyOf(
        const Value& target,
        const std::string& name,
        int line
    ) const;

    void setPropertyOf(
        const Value& target,
        const std::string& name,
        const Value& value,
        int line
    ) const;

    void unwind(const Value& value, int line);

    void callBound(
        const Value& callee,
        const Value& receiver,
        int argCount
    );

    void callMethod(
        const Value& method,
        const Value& target,
        int argCount,
        int line
    );

    [[noreturn]] void throwValue(
        const Value& value,
        int line
    ) const;

    [[noreturn]] void fail(
        const std::string& message,
        int line
    ) const;

    ValueStack valueStack;

    std::vector<CallFrame> frames;
    std::vector<std::shared_ptr<Upvalue>> openUpvalues;

    std::unordered_map<std::string, Value> globals;

    std::vector<Handler> handlers;

    std::unordered_map<std::string, Value> builtins;
    std::shared_ptr<ModuleObject> currentModule;
    std::unordered_map<std::string, Value> loadedModules;

    ModuleLoader* loader = nullptr;
    std::string root = ".";

    bool running = false;
};
