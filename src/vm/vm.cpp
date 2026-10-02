#include "vm.h"

#include "../errors/errors.h"
#include "../runtime/object_list.h"
#include "../runtime/object_class.h"
#include "../runtime/object_module.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../compiler/compiler.h"
#include "../stdlib/stdlib.h"
#include "../runtime/upvalue.h"
#include "../runtime/object_map.h"
#include "../stdlib/stdlib.h"

#include <cmath>

VM::VM() {
}

void VM::defineGlobal(
    const std::string& name,
    const Value& value
) {
    globals[name] = value;
}

void VM::markBuiltins() {
    builtins = globals;
}

bool VM::hasGlobal(const std::string& name) const {
    return globals.find(name) != globals.end();
}

Value VM::getGlobal(const std::string& name) const {
    auto it = globals.find(name);

    if (it == globals.end()) {
        throw RuntimeError(
            "unknown name '" + name + "'"
        );
    }

    return it->second;
}

ValueStack& VM::stack() {
    return valueStack;
}

void VM::interpret(const Chunk& chunk) {
    auto proto = std::make_shared<FunctionProto>(
        "main",
        0,
        0
    );

    proto->chunk() = chunk;

    interpret(proto);
}

void VM::interpret(
    const std::shared_ptr<FunctionProto>& proto
) {
    valueStack.clear();
    frames.clear();
    openUpvalues.clear();

    CallFrame frame;
    frame.closure = nullptr;
    frame.proto = proto;
    frame.ip = 0;
    frame.stackBase = 0;

    frames.push_back(frame);

    for (size_t i = 0; i < static_cast<size_t>(proto->slotCount()); ++i) {
        valueStack.push(Value());
    }

    running = true;

    execute(0);
}

void VM::exportValue(
    const std::string& name,
    const Value& value
) {
    globals[name] = value;
}

ModuleLoader* VM::setModuleLoader(ModuleLoader* next) {
    ModuleLoader* previous = loader;
    loader = next;
    return previous;
}

const std::string& VM::moduleRoot() const {
    return root;
}

void VM::setModuleRoot(const std::string& path) {
    root = path;
}

Value VM::getPropertyOf(
    const Value& target,
    const std::string& name,
    int line
) const {
    if (!target.isObject()) {
        return NovaStd::getProperty(target, name, line);
    }

    auto object = target.asObject();

    if (object->type() == ObjectType::Instance) {
        auto instance = std::static_pointer_cast<InstanceObject>(object);

        if (instance->has(name)) {
            return instance->get(name);
        }

        Value method = instance->definition()->method(name);

        if (method.isObject()) {
            return method;
        }

        return instance->definition()->field(name);
    }

    if (object->type() == ObjectType::Class) {
        auto definition = std::static_pointer_cast<ClassObject>(object);

        if (definition->hasStatic(name)) {
            return definition->staticValue(name);
        }

        fail(
            "class '" + definition->name() + "' has no static field '" +
            name + "'",
            line
        );
    }

    if (object->type() == ObjectType::Module) {
        auto module = std::static_pointer_cast<ModuleObject>(object);

        if (!module->contains(name)) {
            fail(
                "module '" + module->path() +
                "' has no export '" + name + "'",
                line
            );
        }

        return module->get(name);
    }

    if (object->type() == ObjectType::Map) {
        return std::static_pointer_cast<MapObject>(object)->get(name);
    }

    Value method = NovaStd::getProperty(target, name, line);

    if (method.isObject()) {
        return method;
    }

    fail(
        "this value of type " + target.typeName() +
        " has no field '" + name + "'",
        line
    );
}

void VM::setPropertyOf(
    const Value& target,
    const std::string& name,
    const Value& value,
    int line
) const {
    if (!target.isObject()) {
        fail(
            "cannot set field '" + name + "' on a value of type " +
            target.typeName(),
            line
        );
    }

    auto object = target.asObject();

    if (object->type() == ObjectType::Map) {
        std::static_pointer_cast<MapObject>(object)->set(name, value);
        return;
    }

    if (object->type() == ObjectType::Instance) {
        std::static_pointer_cast<InstanceObject>(object)->set(name, value);
        return;
    }

    if (object->type() == ObjectType::Class) {
        std::static_pointer_cast<ClassObject>(object)->defineStatic(
            name,
            value
        );
        return;
    }

    fail(
        "cannot set field '" + name + "' on a value of type " +
        target.typeName(),
        line
    );
}

Value VM::lookupMethod(
    const Value& target,
    const std::string& name,
    const ClassObject* startAfter,
    int line
) const {
    if (!target.isObject()) {
        return NovaStd::getProperty(target, name, line);
    }

    auto object = target.asObject();

    if (object->type() == ObjectType::Instance) {
        auto instance = std::static_pointer_cast<InstanceObject>(object);
        auto definition = instance->definition();

        if (startAfter != nullptr) {
            if (definition.get() == startAfter) {
                definition = definition->parent();
            } else {
                auto walk = definition;

                while (walk != nullptr && walk.get() != startAfter) {
                    walk = walk->parent();
                }

                definition = walk == nullptr ? nullptr : walk->parent();
            }
        }

        if (definition != nullptr) {
            Value method = definition->method(name);

            if (method.isObject()) {
                return method;
            }
        }

        fail(
            "'" + instance->typeName() + "' has no method '" + name + "'",
            line
        );
    }

    if (object->type() == ObjectType::Class) {
        auto definition = std::static_pointer_cast<ClassObject>(object);

        if (startAfter != nullptr && definition.get() == startAfter) {
            definition = definition->parent();
        }

        if (definition != nullptr) {
            Value method = definition->method(name);

            if (method.isObject()) {
                return method;
            }
        }

        fail(
            "class '" +
            std::static_pointer_cast<ClassObject>(object)->name() +
            "' has no method '" + name + "'",
            line
        );
    }

    if (object->type() == ObjectType::Module) {
        auto module = std::static_pointer_cast<ModuleObject>(object);

        if (module->contains(name)) {
            return module->get(name);
        }

        fail(
            "module '" + module->path() +
            "' has no export '" + name + "'",
            line
        );
    }

    return NovaStd::getProperty(target, name, line);
}

void VM::callBound(
    const Value& callee,
    const Value& receiver,
    int argCount
) {
    const Value function = callee;
    const Value self = receiver;

    const size_t count = argCount < 0 ? 0 : static_cast<size_t>(argCount);

    if (valueStack.size() < count) {
        fail("bad call on the stack", 0);
    }

    std::vector<Value> args;

    args.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        args.push_back(
            valueStack.at(valueStack.size() - count + i)
        );
    }

    valueStack.truncate(valueStack.size() - count);
    valueStack.push(function);
    valueStack.push(self);

    for (const Value& argument : args) {
        valueStack.push(argument);
    }

    run(static_cast<int>(count), true);
}

void VM::callMethod(
    const Value& method,
    const Value& target,
    int argCount,
    int line
) {
    bool wantsReceiver = false;

    if (method.isObject() &&
        method.asObject()->type() == ObjectType::Function) {
        wantsReceiver = std::static_pointer_cast<FunctionObject>(
            method.asObject()
        )->proto()->isMethod();
    }

    const Value function = method;
    const Value self = target;

    const size_t count = argCount < 0 ? 0 : static_cast<size_t>(argCount);

    if (valueStack.size() < count + 1) {
        fail("bad call on the stack", line);
    }

    std::vector<Value> args;

    args.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        args.push_back(
            valueStack.at(valueStack.size() - count + i)
        );
    }

    valueStack.truncate(valueStack.size() - count - 1);
    valueStack.push(function);

    if (wantsReceiver) {
        valueStack.push(self);
    }

    for (const Value& argument : args) {
        valueStack.push(argument);
    }

    run(static_cast<int>(count), wantsReceiver);
}

void VM::throwValue(const Value& value, int line) const {
    throw NovaThrow(value, line);
}

void VM::unwind(const Value& value, int line) {
    while (!handlers.empty()) {
        Handler handler = handlers.back();

        if (handler.unwinding) {
            handler.unwinding = false;
            handlers.pop_back();
            continue;
        }

        closeUpvalues(handler.stackDepth);
        valueStack.truncate(handler.stackDepth);

        while (frames.size() > handler.frameIndex + 1) {
            frames.pop_back();
        }

        if (frames.empty()) {
            handlers.pop_back();
            throw NovaThrow(value, line);
        }

        if (handler.cleanupIp != 0 && !handler.hasCatch) {
            handlers.back().pending = value;
            handlers.back().unwinding = true;
            frames.back().ip = handler.cleanupIp;

            return;
        }

        handlers.pop_back();

        frames.back().ip = handler.returnIp;
        valueStack.push(value);

        return;
    }

    throw NovaThrow(value, line);
}

Value VM::display(const Value& value, int line) const {
    if (!value.isObject() ||
        value.asObject()->type() != ObjectType::Instance) {
        return Value(value.toString());
    }

    auto instance =
        std::static_pointer_cast<InstanceObject>(value.asObject());

    const std::shared_ptr<ClassObject>& definition = instance->definition();

    Value method = definition->method("toString");

    if (!method.isObject() ||
        method.asObject()->type() != ObjectType::Function) {
        return Value(value.toString());
    }

    VM* self = const_cast<VM*>(this);

    const size_t depth = self->frames.size();

    self->valueStack.push(method);
    self->valueStack.push(value);

    self->run(0, true);
    self->execute(depth);

    return self->valueStack.pop();
}

Value VM::loadModule(const std::string& path, int line) {
    auto cached = loadedModules.find(path);

    if (cached != loadedModules.end()) {
        return cached->second;
    }

    if (loader == nullptr) {
        fail("this build cannot load modules", line);
    }

    const std::string source = loader->load(path);

    if (source.empty()) {
        fail("module '" + path + "' is empty or missing", line);
    }

    Parser parser(Lexer(source, path).tokenize());
    Compiler compiler;
    auto proto = compiler.compile(parser.parse(), path);

    auto module = std::make_shared<ModuleObject>(path);

    loadedModules[path] = Value(module);

    std::unordered_map<std::string, Value> savedGlobals =
        std::move(globals);
    std::shared_ptr<ModuleObject> savedModule = currentModule;

    globals = builtins;
    currentModule = module;

    const size_t depth = frames.size();
    const size_t base = valueStack.size();

    CallFrame frame;
    frame.closure = nullptr;
    frame.proto = proto;
    frame.ip = 0;
    frame.stackBase = base;

    frames.push_back(frame);

    for (size_t i = 0; i < static_cast<size_t>(proto->slotCount()); ++i) {
        valueStack.push(Value());
    }

    running = true;
    execute(depth);

    if (frames.size() > depth) {
        frames.resize(depth);
    }

    if (valueStack.size() > base) {
        valueStack.truncate(base);
    }

    running = true;

    globals = std::move(savedGlobals);
    currentModule = savedModule;

    return Value(module);
}

Value VM::callFunction(
    const Value& callee,
    std::vector<Value>& args
) {
    size_t depth = frames.size();
    size_t base = valueStack.size();

    valueStack.push(callee);

    for (const auto& argument : args) {
        valueStack.push(argument);
    }

    run(static_cast<int>(args.size()));

    if (frames.size() != depth) {
        execute(depth);
    }

    Value result;

    if (valueStack.size() > base) {
        result = valueStack.pop();
    }

    valueStack.truncate(base);

    return result;
}

void VM::execute(size_t stopDepth) {
    const size_t handlerFloor = handlers.size();

    while (running && frames.size() > stopDepth) {
        if (frames.empty()) {
            running = false;
            break;
        }

        const Chunk& chunk = frames.back().proto->chunk();

        if (frames.back().ip >= chunk.count()) {
            fail(
                "instruction pointer went past the end of '" +
                frames.back().proto->name() + "'",
                0
            );
        }

        const Instruction& instruction =
            chunk.code()[frames.back().ip++];

        switch (instruction.op) {
            case OpCode::Halt:
                running = false;
                break;

            case OpCode::Constant: {
                const Value& constant =
                    require(instruction.operand, instruction.line);

                if (constant.isObject() &&
                    constant.asObject()->type() == ObjectType::Function) {
                    valueStack.push(
                        makeClosure(
                            std::static_pointer_cast<FunctionProto>(
                                constant.asObject()
                            )
                        )
                    );

                    break;
                }

                valueStack.push(constant);
                break;
            }

            case OpCode::Nil:
                valueStack.push(Value());
                break;

            case OpCode::True:
                valueStack.push(Value(true));
                break;

            case OpCode::False:
                valueStack.push(Value(false));
                break;

            case OpCode::Pop:
                valueStack.pop();
                break;

            case OpCode::GetLocal:
                valueStack.push(
                    local(instruction.operand)
                );
                break;

            case OpCode::SetLocal:
                setLocal(
                    instruction.operand,
                    valueStack.top()
                );
                break;

            case OpCode::GetUpvalue: {
                const auto& closure = frames.back().closure;

                if (!closure) {
                    fail("upvalue used outside a function", instruction.line);
                }

                valueStack.push(
                    closure->upvalue(instruction.operand)->get()
                );

                break;
            }

            case OpCode::SetUpvalue: {
                const auto& closure = frames.back().closure;

                if (!closure) {
                    fail("upvalue used outside a function", instruction.line);
                }

                closure->upvalue(instruction.operand)->set(
                    valueStack.top()
                );

                break;
            }

            case OpCode::CloseUpvalue:
                closeUpvalues(
                    frames.back().stackBase +
                    instruction.operand
                );
                break;

            case OpCode::GetGlobal: {
                const Value& name =
                    require(instruction.operand, instruction.line);

                if (!name.isString()) {
                    fail("internal error: name is not a string", instruction.line);
                }

                auto it = globals.find(name.asString());

                if (it == globals.end()) {
                    fail(
                        "unknown variable '" + name.asString() + "'",
                        instruction.line
                    );
                }

                valueStack.push(it->second);
                break;
            }

            case OpCode::SetGlobal: {
                const Value& name =
                    require(instruction.operand, instruction.line);

                if (!name.isString()) {
                    fail("internal error: name is not a string", instruction.line);
                }

                globals[name.asString()] = valueStack.top();
                break;
            }

            case OpCode::GetProperty: {
                Value target = valueStack.pop();
                const Value& name =
                    require(instruction.operand, instruction.line);

                valueStack.push(
                    getPropertyOf(target, name.asString(), instruction.line)
                );

                break;
            }

            case OpCode::SetProperty: {
                Value value = valueStack.pop();
                Value target = valueStack.pop();

                const Value& name =
                    require(instruction.operand, instruction.line);

                setPropertyOf(
                    target,
                    name.asString(),
                    value,
                    instruction.line
                );

                valueStack.push(value);
                break;
            }

            case OpCode::Index: {
                Value index = valueStack.pop();
                Value target = valueStack.pop();

                valueStack.push(
                    indexOf(target, index, instruction.line)
                );

                break;
            }

            case OpCode::SetIndex: {
                Value value = valueStack.pop();
                Value index = valueStack.pop();
                Value target = valueStack.pop();

                if (target.isString()) {
                    fail(
                        "strings cannot be changed after they are created",
                        instruction.line
                    );
                }

                if (!target.isObject()) {
                    fail(
                        "only lists and maps support index assignment",
                        instruction.line
                    );
                }

                auto object = target.asObject();

                if (object->type() == ObjectType::List) {
                    auto list = std::static_pointer_cast<ListObject>(object);

                    if (!index.isNumber()) {
                        fail("list index must be a number", instruction.line);
                    }

                    double position = index.asNumber();

                    if (
                        position < 0 ||
                        position != std::floor(position)
                    ) {
                        fail(
                            "list index must be a whole number",
                            instruction.line
                        );
                    }

                    list->set(
                        static_cast<size_t>(position),
                        value
                    );

                    valueStack.push(value);
                    break;
                }

                if (object->type() == ObjectType::Map) {
                    auto map = std::static_pointer_cast<MapObject>(object);

                    map->set(mapKeyOf(index), value);
                    valueStack.push(value);
                    break;
                }

                fail(
                    "only lists and maps support index assignment",
                    instruction.line
                );
            }

            case OpCode::BuildList: {
                size_t count = static_cast<size_t>(instruction.operand);
                size_t base = valueStack.size() - count;

                std::vector<Value> items(
                    valueStack.begin() + static_cast<long>(base),
                    valueStack.end()
                );

                valueStack.truncate(base);

                valueStack.push(
                    Value(
                        std::make_shared<ListObject>(
                            std::move(items)
                        )
                    )
                );

                break;
            }

            case OpCode::BuildMap: {
                size_t count = static_cast<size_t>(instruction.operand);
                size_t base = valueStack.size() - count * 2;

                std::vector<std::pair<std::string, Value>> entries;

                for (size_t i = 0; i < count; ++i) {
                    const Value& key =
                        valueStack.at(base + i * 2);

                    const Value& value =
                        valueStack.at(base + i * 2 + 1);

                    entries.emplace_back(mapKeyOf(key), value);
                }

                valueStack.truncate(base);

                valueStack.push(
                    Value(
                        std::make_shared<MapObject>(
                            std::move(entries)
                        )
                    )
                );

                break;
            }

            case OpCode::ToList: {
                Value target = valueStack.pop();
                valueStack.push(toList(target, instruction.line));
                break;
            }

            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide:
            case OpCode::Modulo:
            case OpCode::Equal:
            case OpCode::NotEqual:
            case OpCode::Greater:
            case OpCode::GreaterEqual:
            case OpCode::Less:
            case OpCode::LessEqual: {
                Value right = valueStack.pop();
                Value left = valueStack.pop();

                valueStack.push(
                    binary(
                        instruction.op,
                        left,
                        right,
                        instruction.line
                    )
                );

                break;
            }

            case OpCode::Negate: {
                Value value = valueStack.pop();

                if (!value.isNumber()) {
                    fail(
                        "cannot negate a value of type " + value.typeName(),
                        instruction.line
                    );
                }

                valueStack.push(
                    Value(-value.asNumber())
                );

                break;
            }

            case OpCode::Not: {
                Value value = valueStack.pop();
                valueStack.push(Value(!value.isTruthy()));
                break;
            }

            case OpCode::Jump:
                frames.back().ip =
                    static_cast<size_t>(instruction.operand);
                break;

            case OpCode::JumpIfFalse:
                if (!valueStack.top().isTruthy()) {
                    frames.back().ip =
                        static_cast<size_t>(instruction.operand);
                }
                break;

            case OpCode::JumpIfTrue:
                if (valueStack.top().isTruthy()) {
                    frames.back().ip =
                        static_cast<size_t>(instruction.operand);
                }
                break;

            case OpCode::Loop:
                frames.back().ip =
                    static_cast<size_t>(instruction.operand);
                break;

            case OpCode::Call:
                run(instruction.operand);
                break;

            case OpCode::DefineClass: {
                const Value& name =
                    require(instruction.operand, instruction.line);

                valueStack.push(
                    Value(std::make_shared<ClassObject>(name.asString()))
                );

                break;
            }

            case OpCode::InheritClass: {
                Value base = valueStack.pop();
                const Value& name =
                    require(instruction.operand, instruction.line);

                if (!base.isObject() ||
                    base.asObject()->type() != ObjectType::Class) {
                    fail(
                        "a class can only extend another class, not " +
                        base.typeName(),
                        instruction.line
                    );
                }

                valueStack.push(
                    Value(std::make_shared<ClassObject>(
                        name.asString(),
                        std::static_pointer_cast<ClassObject>(
                            base.asObject()
                        )
                    ))
                );

                break;
            }

            case OpCode::Method: {
                const int count = instruction.operand;

                if (valueStack.size() <
                    static_cast<size_t>(count) * 2 + 1) {
                    fail("bad class field setup", instruction.line);
                }

                auto definition = std::static_pointer_cast<ClassObject>(
                    valueStack.at(
                        valueStack.size() - static_cast<size_t>(count) * 2
                            - 1
                    ).asObject()
                );

                for (int i = 0; i < count; ++i) {
                    Value value = valueStack.pop();
                    Value key = valueStack.pop();

                    definition->defineField(key.asString(), value);
                }

                break;
            }

            case OpCode::DefineStatic:
            case OpCode::BindMethod: {
                Value method = valueStack.pop();
                const Value& name =
                    require(instruction.operand, instruction.line);

                if (valueStack.empty()) {
                    fail("bad class setup", instruction.line);
                }

                auto definition = std::static_pointer_cast<ClassObject>(
                    valueStack.at(valueStack.size() - 1).asObject()
                );

                if (instruction.op == OpCode::DefineStatic) {
                    definition->defineStatic(name.asString(), method);
                } else {
                    definition->defineMethod(name.asString(), method);
                }

                break;
            }

            case OpCode::Construct: {
                const int argCount = instruction.operand;

                if (valueStack.size() <
                    static_cast<size_t>(argCount) + 1) {
                    fail("bad class setup", instruction.line);
                }

                const size_t classIndex =
                    valueStack.size() - static_cast<size_t>(argCount) - 1;

                Value target = valueStack.at(classIndex);

                if (!target.isObject() ||
                    target.asObject()->type() != ObjectType::Class) {
                    fail(
                        "'new' needs a class, not " + target.typeName(),
                        instruction.line
                    );
                }

                auto definition =
                    std::static_pointer_cast<ClassObject>(
                        target.asObject()
                    );

                auto instance = std::make_shared<InstanceObject>(
                    definition
                );

                Value constructor = definition->constructor();

                if (!constructor.isObject() &&
                    definition->parent() != nullptr) {
                    constructor = definition->parent()->constructor();
                }

                if (!constructor.isObject()) {
                    if (argCount != 0) {
                        fail(
                            "'" + definition->name() +
                            "' has no constructor to take arguments",
                            instruction.line
                        );
                    }

                    valueStack.truncate(classIndex);
                    valueStack.push(Value(instance));
                    break;
                }

                std::vector<Value> arguments;
                arguments.reserve(static_cast<size_t>(argCount));

                for (int i = 0; i < argCount; ++i) {
                    arguments.push_back(
                        valueStack.at(classIndex +
                            static_cast<size_t>(i) + 1)
                    );
                }

                valueStack.truncate(classIndex);

                for (const Value& argument : arguments) {
                    valueStack.push(argument);
                }

                callBound(constructor, Value(instance), argCount);

                valueStack.set(valueStack.size() - 1, Value(instance));
                break;
            }

            case OpCode::Invoke: {
                const int argCount = instruction.operand & 0xFF;
                const int nameIndex = instruction.operand >> 8;
                const Value& name =
                    require(nameIndex, instruction.line);

                size_t base = valueStack.size() -
                    static_cast<size_t>(argCount) - 1;

                Value target = valueStack.at(base);

                Value method = lookupMethod(
                    target,
                    name.asString(),
                    nullptr,
                    instruction.line
                );

                callMethod(method, target, argCount, instruction.line);
                break;
            }

            case OpCode::SuperInvoke: {
                const int argCount = instruction.operand & 0xFF;
                const int nameIndex = instruction.operand >> 8;
                const Value& name =
                    require(nameIndex, instruction.line);

                if (frames.empty() ||
                    !frames.back().proto->isMethod()) {
                    fail(
                        "'super' only works inside a method",
                        instruction.line
                    );
                }

                const size_t argStart = valueStack.size() -
                    static_cast<size_t>(argCount);

                if (argStart < 2) {
                    fail("bad 'super' call", instruction.line);
                }

                Value target = valueStack.at(argStart - 1);
                Value owner = valueStack.at(argStart - 2);

                if (!owner.isObject() ||
                    owner.asObject()->type() != ObjectType::Class) {
                    fail("bad 'super' call", instruction.line);
                }

                auto definition =
                    std::static_pointer_cast<ClassObject>(
                        owner.asObject()
                    );

                if (definition->parent() == nullptr) {
                    fail(
                        "'" + definition->name() +
                        "' has no base class to call",
                        instruction.line
                    );
                }

                Value method =
                    definition->parent()->method(name.asString());

                if (!method.isObject()) {
                    fail(
                        "'" + definition->parent()->name() +
                        "' has no method '" + name.asString() + "'",
                        instruction.line
                    );
                }

                std::vector<Value> arguments;
                arguments.reserve(static_cast<size_t>(argCount));

                for (int i = 0; i < argCount; ++i) {
                    arguments.push_back(
                        valueStack.at(argStart + static_cast<size_t>(i))
                    );
                }

                valueStack.truncate(argStart - 2);

                for (const Value& argument : arguments) {
                    valueStack.push(argument);
                }

                callMethod(method, target, argCount, instruction.line);
                break;
            }

            case OpCode::SuperCall: {
                const int argCount = instruction.operand;

                if (frames.empty() ||
                    !frames.back().proto->isMethod()) {
                    fail(
                        "'super' only works inside a method",
                        instruction.line
                    );
                }

                const size_t argStart = valueStack.size() -
                    static_cast<size_t>(argCount);

                if (argStart < 2) {
                    fail("bad 'super' call", instruction.line);
                }

                const Value owner = valueStack.at(argStart - 2);
                const Value target = valueStack.at(argStart - 1);

                if (!owner.isObject() ||
                    owner.asObject()->type() != ObjectType::Class) {
                    fail("bad 'super' call", instruction.line);
                }

                auto definition =
                    std::static_pointer_cast<ClassObject>(
                        owner.asObject()
                    );

                if (definition->parent() == nullptr) {
                    fail(
                        "'" + definition->name() +
                        "' has no base class to call",
                        instruction.line
                    );
                }

                Value constructor =
                    definition->parent()->constructor();

                if (!constructor.isObject()) {
                    fail(
                        "the base class of '" + definition->name() +
                        "' has no constructor",
                        instruction.line
                    );
                }

                std::vector<Value> arguments;
                arguments.reserve(static_cast<size_t>(argCount));

                for (int i = 0; i < argCount; ++i) {
                    arguments.push_back(
                        valueStack.at(argStart + static_cast<size_t>(i))
                    );
                }

                valueStack.truncate(argStart - 1);

                for (const Value& argument : arguments) {
                    valueStack.push(argument);
                }

                callBound(constructor, target, argCount);

                valueStack.set(valueStack.size() - 1, target);
                break;
            }

            case OpCode::LoadModule: {
                Value path = valueStack.pop();

                valueStack.push(
                    loadModule(path.asString(), instruction.line)
                );

                break;
            }

            case OpCode::Export: {
                const int count = instruction.operand;

                if (currentModule == nullptr) {
                    fail(
                        "'export' only works inside a module",
                        instruction.line
                    );
                }

                std::vector<std::pair<std::string, Value>> pairs;

                for (int i = 0; i < count; ++i) {
                    Value key = valueStack.pop();
                    Value value = valueStack.pop();
                    pairs.emplace_back(key.asString(), value);
                }

                for (const auto& pair : pairs) {
                    currentModule->exportValue(pair.first, pair.second);
                }

                break;
            }

            case OpCode::Throw:
            case OpCode::Rethrow: {
                Value value = instruction.op == OpCode::Rethrow
                    ? handlers.back().pending
                    : valueStack.pop();

                if (instruction.op == OpCode::Rethrow) {
                    if (!handlers.empty() &&
                        handlers.back().cleanupIp != 0) {
                        handlers.back().cleanupIp = 0;
                    }
                }

                unwind(value, instruction.line);
                break;
            }

            case OpCode::PushHandler: {
                Handler handler;

                handler.frameIndex = frames.size() - 1;
                handler.stackDepth = valueStack.size();
                handler.returnIp = static_cast<size_t>(
                    instruction.operand & 0x3FFFFFFF
                );
                handler.hasCatch = (instruction.operand & 0x40000000) != 0;

                handlers.push_back(handler);
                break;
            }

            case OpCode::SetCleanup: {
                if (!handlers.empty()) {
                    handlers.back().cleanupIp =
                        static_cast<size_t>(instruction.operand);
                }

                break;
            }

            case OpCode::EndFinally: {
                if (handlers.empty() || !handlers.back().unwinding) {
                    break;
                }

                Handler handler = handlers.back();
                handlers.pop_back();

                unwind(handler.pending, instruction.line);
                break;
            }

            case OpCode::PopHandler: {
                if (handlers.size() > handlerFloor) {
                    handlers.pop_back();
                }

                break;
            }

            case OpCode::Return: {
                Value result = valueStack.pop();

                size_t base = frames.back().stackBase;

                closeUpvalues(base);
                frames.pop_back();

                valueStack.truncate(base);
                valueStack.push(result);

                if (frames.empty()) {
                    running = false;
                }

                break;
            }
        }
    }
}

Value VM::makeClosure(
    const std::shared_ptr<FunctionProto>& proto
) {
    auto function = std::make_shared<FunctionObject>(proto);

    for (size_t i = 0; i < proto->upvalues().size(); ++i) {
        const UpvalueSlot& slot = proto->upvalues()[i];

        if (slot.isLocal) {
            function->bindUpvalue(
                static_cast<int>(i),
                captureUpvalue(
                    frames.back().stackBase +
                    static_cast<size_t>(slot.index)
                )
            );

            continue;
        }

        const auto& closure = frames.back().closure;

        if (!closure) {
            fail(
                "internal error: a closure lost its surrounding function",
                0
            );
        }

        function->bindUpvalue(
            static_cast<int>(i),
            closure->upvalue(slot.index)
        );
    }

    return Value(
        std::static_pointer_cast<Object>(function)
    );
}

void VM::run(int argCount, bool receiverOnStack) {
    const size_t fixed =
        static_cast<size_t>(argCount < 0 ? 0 : argCount) +
        (receiverOnStack ? 2 : 1);

    if (argCount < 0 || valueStack.size() < fixed) {
        fail("bad call on the stack", 0);
    }

    const size_t calleeIndex = valueStack.size() - fixed;
    const size_t firstArg = calleeIndex + (receiverOnStack ? 2 : 1);

    Value callee = valueStack.at(calleeIndex);

    if (!callee.isObject()) {
        fail(
            "this value of type " + callee.typeName() + " is not a function",
            0
        );
    }

    auto object = callee.asObject();

    if (object->type() == ObjectType::Native) {
        auto native = std::static_pointer_cast<NativeObject>(object);

        std::vector<Value> args;
        args.reserve(static_cast<size_t>(argCount));

        for (int i = 0; i < argCount; ++i) {
            args.push_back(valueStack.at(firstArg + static_cast<size_t>(i)));
        }

        valueStack.truncate(calleeIndex);

        Value result = native->call(*this, args);

        valueStack.push(result);
        return;
    }

    if (object->type() != ObjectType::Function) {
        fail(
            "this value of type " + callee.typeName() + " is not a function",
            0
        );
    }

    auto function = std::static_pointer_cast<FunctionObject>(object);

    if (function->proto()->isMethod() && !receiverOnStack) {
        fail(
            "'" + function->name() +
            "' is a method, call it on a value",
            0
        );
    }

    const int expected =
        function->arity() -
        (function->proto()->isMethod() ? 1 : 0);

    if (argCount != expected) {
        fail(
            "'" + function->name() + "' expects " +
            std::to_string(expected) +
            " arguments but got " +
            std::to_string(argCount),
            0
        );
    }

    const auto& proto = function->proto();

    std::vector<Value> arguments;
    arguments.reserve(static_cast<size_t>(argCount));

    for (int i = 0; i < argCount; ++i) {
        arguments.push_back(valueStack.at(firstArg + static_cast<size_t>(i)));
    }

    if (receiverOnStack) {
        const Value self = valueStack.at(calleeIndex + 1);

        for (size_t i = 0; i < arguments.size(); ++i) {
            valueStack.set(calleeIndex + 1 + i, arguments[i]);
        }

        valueStack.set(calleeIndex, self);
    } else {
        for (size_t i = 0; i < arguments.size(); ++i) {
            valueStack.set(calleeIndex + i, arguments[i]);
        }
    }

    valueStack.truncate(
        calleeIndex + static_cast<size_t>(argCount) +
        (receiverOnStack ? 1 : 0)
    );

    CallFrame frame;
    frame.closure = function;
    frame.proto = proto;
    frame.ip = 0;
    frame.stackBase = calleeIndex;

    frames.push_back(frame);

    for (size_t i = static_cast<size_t>(argCount);
         i < static_cast<size_t>(proto->slotCount());
         ++i) {
        valueStack.push(Value());
    }
}

std::shared_ptr<Upvalue> VM::captureUpvalue(
    size_t stackIndex
) {
    Value* slot = &valueStack.at(stackIndex);

    for (const auto& upvalue : openUpvalues) {
        if (upvalue->isOpen() && upvalue->slot() == slot) {
            return upvalue;
        }
    }

    auto upvalue = std::make_shared<Upvalue>(slot);

    openUpvalues.push_back(upvalue);

    return upvalue;
}

void VM::closeUpvalues(size_t stackIndex) {
    Value* first = valueStack.data() + stackIndex;

    for (size_t i = 0; i < openUpvalues.size();) {
        if (openUpvalues[i]->isOpen() &&
            openUpvalues[i]->slot() >= first) {
            openUpvalues[i]->close();
            openUpvalues.erase(openUpvalues.begin() + static_cast<long>(i));
            continue;
        }

        i++;
    }
}

Value VM::local(int slot) const {
    const CallFrame& frame = frames.back();

    if (slot < 0 || slot >= frame.proto->slotCount()) {
        fail("bad local slot " + std::to_string(slot), 0);
    }

    return valueStack.at(frame.stackBase + static_cast<size_t>(slot));
}

void VM::setLocal(int slot, const Value& value) {
    CallFrame& frame = frames.back();

    if (slot < 0 || slot >= frame.proto->slotCount()) {
        fail("bad local slot " + std::to_string(slot), 0);
    }

    valueStack.set(
        frame.stackBase + static_cast<size_t>(slot),
        value
    );
}

const Value& VM::require(
    int constantIndex,
    int line
) const {
    const auto& constants =
        frames.back().proto->chunk().constants();

    if (
        constantIndex < 0 ||
        static_cast<size_t>(constantIndex) >= constants.size()
    ) {
        fail("bad constant index " + std::to_string(constantIndex), line);
    }

    return constants[constantIndex];
}

static std::string operatorSymbol(OpCode op) {
    switch (op) {
        case OpCode::Add: return "+";
        case OpCode::Subtract: return "-";
        case OpCode::Multiply: return "*";
        case OpCode::Divide: return "/";
        case OpCode::Modulo: return "%";
        case OpCode::Equal: return "==";
        case OpCode::NotEqual: return "!=";
        case OpCode::Greater: return ">";
        case OpCode::GreaterEqual: return ">=";
        case OpCode::Less: return "<";
        default: return "<=";
    }
}

Value VM::binary(
    OpCode op,
    const Value& left,
    const Value& right,
    int line
) const {
    if (op == OpCode::Equal) {
        return Value(left.equals(right));
    }

    if (op == OpCode::NotEqual) {
        return Value(!left.equals(right));
    }

    if (op == OpCode::Add) {
        if (left.isNumber() && right.isNumber()) {
            return Value(left.asNumber() + right.asNumber());
        }

        if (left.isString() && right.isString()) {
            return Value(left.asString() + right.asString());
        }

        if (left.isString()) {
            return Value(left.asString() + right.toString());
        }

        if (right.isString()) {
            return Value(left.toString() + right.asString());
        }

        if (
            left.isObject() &&
            left.asObject()->type() == ObjectType::List &&
            right.isObject() &&
            right.asObject()->type() == ObjectType::List
        ) {
            auto first =
                std::static_pointer_cast<ListObject>(left.asObject());
            auto second =
                std::static_pointer_cast<ListObject>(right.asObject());

            std::vector<Value> items = first->elements();
            items.insert(
                items.end(),
                second->elements().begin(),
                second->elements().end()
            );

            return Value(
                std::make_shared<ListObject>(std::move(items))
            );
        }

        fail(
            "cannot use + with " + left.typeName() +
            " and " + right.typeName(),
            line
        );
    }

    bool comparing = op == OpCode::Greater ||
        op == OpCode::GreaterEqual ||
        op == OpCode::Less ||
        op == OpCode::LessEqual;

    if (comparing && left.isString() && right.isString()) {
        int order = left.asString().compare(right.asString());

        switch (op) {
            case OpCode::Greater:
                return Value(order > 0);
            case OpCode::GreaterEqual:
                return Value(order >= 0);
            case OpCode::Less:
                return Value(order < 0);
            default:
                return Value(order <= 0);
        }
    }

    if (!left.isNumber() || !right.isNumber()) {
        std::string symbol = operatorSymbol(op);

        if (comparing) {
            fail(
                "cannot compare " + left.typeName() + " with " +
                right.typeName() + " using " + symbol,
                line
            );
        }

        fail(
            "cannot use " + symbol + " with " + left.typeName() +
            " and " + right.typeName(),
            line
        );
    }

    double a = left.asNumber();
    double b = right.asNumber();

    switch (op) {
        case OpCode::Subtract:
            return Value(a - b);

        case OpCode::Multiply:
            return Value(a * b);

        case OpCode::Divide:
            if (b == 0) {
                fail("division by zero", line);
            }

            return Value(a / b);

        case OpCode::Modulo:
            if (b == 0) {
                fail("remainder by zero", line);
            }

            return Value(std::fmod(a, b));

        case OpCode::Greater:
            return Value(a > b);

        case OpCode::GreaterEqual:
            return Value(a >= b);

        case OpCode::Less:
            return Value(a < b);

        case OpCode::LessEqual:
            return Value(a <= b);

        default:
            break;
    }

    fail("unknown operator", line);
}

Value VM::indexOf(
    const Value& target,
    const Value& index,
    int line
) const {
    if (target.isString()) {
        if (!index.isNumber()) {
            fail("string index must be a number", line);
        }

        double position = index.asNumber();

        if (
            position < 0 ||
            position != std::floor(position)
        ) {
            fail(
                "string index must be a whole number",
                line
            );
        }

        if (
            position >= static_cast<double>(target.asString().size())
        ) {
            fail(
                "string index " + formatNumber(position) + " out of range",
                line
            );
        }

        return Value(
            std::string(1, target.asString()[static_cast<size_t>(position)])
        );
    }

    if (target.isObject()) {
        auto object = target.asObject();

        if (object->type() == ObjectType::List) {
            auto list = std::static_pointer_cast<ListObject>(object);

            if (!index.isNumber()) {
                fail("list index must be a number", line);
            }

            double position = index.asNumber();

            if (
                position < 0 ||
                position != std::floor(position)
            ) {
                fail("list index must be a whole number", line);
            }

            return list->get(static_cast<size_t>(position));
        }

        if (object->type() == ObjectType::Map) {
            auto map = std::static_pointer_cast<MapObject>(object);
            return map->get(mapKeyOf(index));
        }
    }

    fail(
        "cannot index a value of type " + target.typeName(),
        line
    );
}

Value VM::toList(const Value& value, int line) const {
    if (value.isString()) {
        std::vector<Value> items;

        for (char c : value.asString()) {
            items.push_back(Value(std::string(1, c)));
        }

        return Value(
            std::make_shared<ListObject>(std::move(items))
        );
    }

    if (value.isObject()) {
        auto object = value.asObject();

        if (object->type() == ObjectType::List) {
            return value;
        }

        if (object->type() == ObjectType::Map) {
            auto map = std::static_pointer_cast<MapObject>(object);

            std::vector<Value> items;

            for (const auto& key : map->keys()) {
                items.push_back(Value(key));
            }

            return Value(
                std::make_shared<ListObject>(std::move(items))
            );
        }
    }

    fail(
        "cannot iterate over a value of type " + value.typeName(),
        line
    );
}

std::string VM::stackTrace() const {
    std::string out;

    for (size_t i = frames.size(); i > 0; --i) {
        const CallFrame& frame = frames[i - 1];
        const Chunk& chunk = frame.proto->chunk();

        size_t ip = frame.ip > 0 ? frame.ip - 1 : 0;

        int line = 0;

        if (ip < chunk.count()) {
            line = chunk.code()[ip].line;
        }

        out += "  in " + frame.proto->name();

        if (line > 0) {
            out += " at line " + std::to_string(line);
        }

        out += "\n";
    }

    return out;
}

void VM::fail(
    const std::string& message,
    int line
) const {
    throw RuntimeError(message, { line, 0 });
}
