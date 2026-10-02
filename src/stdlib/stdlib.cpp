#include "stdlib.h"

#include "../io/file_io.h"
#include "../net/net.h"

#include "../errors/errors.h"
#include "../runtime/object_list.h"
#include "../runtime/object_map.h"
#include "../runtime/object_native.h"
#include "../vm/vm.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <random>
#include <string>

namespace NovaStd {

using Handler = NativeObject::Handler;

namespace {

struct MethodEntry {
    std::string name;
    int minArgs;
    int maxArgs;
};

double numberOf(
    const Value& value,
    const char* where
) {
    if (!value.isNumber()) {
        throw RuntimeError(
            std::string(where) + " expects a number, got " +
            value.typeName()
        );
    }

    return value.asNumber();
}

const std::string& stringOf(
    const Value& value,
    const char* where
) {
    if (!value.isString()) {
        throw RuntimeError(
            std::string(where) + " expects a string, got " +
            value.typeName()
        );
    }

    return value.asString();
}

Value toNumber(const Value& value) {
    if (value.isNumber()) {
        return value;
    }

    if (value.isBool()) {
        return Value(value.asBool() ? 1.0 : 0.0);
    }

    if (!value.isString()) {
        throw RuntimeError(
            "num expects a string, got " + value.typeName()
        );
    }

    const std::string& text = value.asString();

    try {
        size_t used = 0;
        double result = std::stod(text, &used);

        while (used < text.size() &&
               std::isspace(static_cast<unsigned char>(text[used]))) {
            used++;
        }

        if (used == text.size()) {
            return Value(result);
        }
    } catch (const std::exception&) {
        throw RuntimeError(
            "cannot read '" + text + "' as a number"
        );
    }

    throw RuntimeError(
        "cannot read '" + text + "' as a number"
    );
}

Value lengthOf(const Value& value) {
    if (value.isString()) {
        return Value(
            static_cast<double>(value.asString().size())
        );
    }

    if (value.isObject()) {
        auto object = value.asObject();

        if (object->type() == ObjectType::List) {
            auto list = std::static_pointer_cast<ListObject>(object);

            return Value(
                static_cast<double>(list->elements().size())
            );
        }

        if (object->type() == ObjectType::Map) {
            auto map = std::static_pointer_cast<MapObject>(object);

            return Value(static_cast<double>(map->size()));
        }
    }

    throw RuntimeError(
        "len expects a string, list or map, got " + value.typeName()
    );
}

std::vector<Value> charsOf(const std::string& text) {
    std::vector<Value> out;

    for (char c : text) {
        out.push_back(Value(std::string(1, c)));
    }

    return out;
}

Value joinWith(
    const Value& separator,
    const std::vector<Value>& items
) {
    std::string out;

    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out += separator.toString();
        }

        out += items[i].toString();
    }

    return Value(out);
}

Value cut(
    const std::string& text,
    double from,
    double length
) {
    if (from < 0 || length < 0) {
        throw RuntimeError(
            "the start and length must not be negative"
        );
    }

    size_t start = static_cast<size_t>(from);

    if (start >= text.size()) {
        return Value("");
    }

    size_t count = static_cast<size_t>(length);

    if (start + count > text.size()) {
        count = text.size() - start;
    }

    return Value(text.substr(start, count));
}

Value wholeIndex(
    const Value& value,
    const char* where
) {
    double position = numberOf(value, where);

    if (position < 0 || position != std::floor(position)) {
        throw RuntimeError(
            std::string(where) + " needs a whole number"
        );
    }

    return Value(position);
}

std::mt19937& engine() {
    static std::mt19937 generator(20240101);
    return generator;
}

std::shared_ptr<NativeObject> function(
    const std::string& name,
    int minArgs,
    int maxArgs,
    Handler handler
) {
    return std::make_shared<NativeObject>(
        name,
        minArgs,
        maxArgs,
        std::move(handler)
    );
}

void define(
    VM& vm,
    const std::string& name,
    int minArgs,
    int maxArgs,
    Handler handler
) {
    vm.defineGlobal(
        name,
        Value(function(name, minArgs, maxArgs, std::move(handler)))
    );
}

Value callWith(
    VM& vm,
    const Value& callee,
    std::vector<Value>& args
) {
    return vm.callFunction(callee, args);
}

Value printValues(VM& machine, std::vector<Value>& args) {
    std::string line;

    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            line += " ";
        }

        line += machine.display(args[i]).toString();
    }

    std::cout << line << '\n';

    return Value();
}

Value makeRange(VM&, std::vector<Value>& args) {
    double from = 0;
    double to = 0;
    double step = 1;

    if (args.size() == 1) {
        to = numberOf(args[0], "range");
    } else if (args.size() == 2) {
        from = numberOf(args[0], "range");
        to = numberOf(args[1], "range");
    } else if (args.size() == 3) {
        from = numberOf(args[0], "range");
        to = numberOf(args[1], "range");
        step = numberOf(args[2], "range");
    } else {
        throw RuntimeError("range expects one to three numbers");
    }

    if (step == 0) {
        throw RuntimeError("the range step cannot be zero");
    }

    std::vector<Value> items;

    if (step > 0) {
        for (double at = from; at < to; at += step) {
            items.push_back(Value(at));
        }
    } else {
        for (double at = from; at > to; at += step) {
            items.push_back(Value(at));
        }
    }

    return Value(
        std::make_shared<ListObject>(std::move(items))
    );
}

Value sumOf(VM&, std::vector<Value>& args) {
    if (args.size() == 1 && args[0].isObject() &&
        args[0].asObject()->type() == ObjectType::List) {
        args = std::static_pointer_cast<ListObject>(
            args[0].asObject()
        )->elements();
    }

    double total = 0;

    for (const auto& item : args) {
        total += numberOf(item, "sum");
    }

    return Value(total);
}

Value stringMethod(
    const std::string& name,
    const Value& self,
    std::vector<Value>& args
) {
    const std::string& text = self.asString();

    if (name == "len") {
        return Value(static_cast<double>(text.size()));
    }

    if (name == "upper" || name == "lower") {
        std::string out = text;

        for (char& c : out) {
            unsigned char raw = static_cast<unsigned char>(c);

            c = static_cast<char>(
                name == "upper" ? std::toupper(raw) : std::tolower(raw)
            );
        }

        return Value(out);
    }

    if (name == "trim") {
        size_t start = 0;
        size_t end = text.size();

        while (start < end &&
               std::isspace(static_cast<unsigned char>(text[start]))) {
            start++;
        }

        while (end > start &&
               std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            end--;
        }

        return Value(text.substr(start, end - start));
    }

    if (name == "split") {
        const std::string& separator = stringOf(args[0], "split");

        std::vector<Value> parts;

        if (separator.empty()) {
            return Value(
                std::make_shared<ListObject>(charsOf(text))
            );
        }

        size_t from = 0;

        while (true) {
            size_t at = text.find(separator, from);

            if (at == std::string::npos) {
                parts.push_back(Value(text.substr(from)));
                break;
            }

            parts.push_back(Value(text.substr(from, at - from)));
            from = at + separator.size();
        }

        return Value(
            std::make_shared<ListObject>(std::move(parts))
        );
    }

    if (name == "contains") {
        return Value(
            text.find(stringOf(args[0], "contains")) !=
            std::string::npos
        );
    }

    if (name == "startsWith") {
        const std::string& prefix = stringOf(args[0], "startsWith");

        return Value(text.compare(0, prefix.size(), prefix) == 0);
    }

    if (name == "endsWith") {
        const std::string& suffix = stringOf(args[0], "endsWith");

        if (suffix.size() > text.size()) {
            return Value(false);
        }

        return Value(
            text.compare(
                text.size() - suffix.size(),
                suffix.size(),
                suffix
            ) == 0
        );
    }

    if (name == "replace") {
        const std::string& from = stringOf(args[0], "replace");
        const std::string& to = stringOf(args[1], "replace");

        if (from.empty()) {
            return Value(text);
        }

        std::string out;
        size_t position = 0;

        while (true) {
            size_t at = text.find(from, position);

            if (at == std::string::npos) {
                out += text.substr(position);
                break;
            }

            out += text.substr(position, at - position);
            out += to;
            position = at + from.size();
        }

        return Value(out);
    }

    if (name == "indexOf") {
        size_t at = text.find(stringOf(args[0], "indexOf"));

        return Value(at == std::string::npos
            ? -1.0
            : static_cast<double>(at));
    }

    if (name == "substring") {
        double start = wholeIndex(args[0], "substring").asNumber();

        double end = args.size() > 1
            ? wholeIndex(args[1], "substring").asNumber()
            : static_cast<double>(text.size());

        return cut(text, start, end - start);
    }

    if (name == "repeat") {
        double count = numberOf(args[0], "repeat");

        if (count < 0) {
            throw RuntimeError("repeat needs a count of zero or more");
        }

        std::string out;

        for (double i = 0; i < count; i++) {
            out += text;
        }

        return Value(out);
    }

    if (name == "chars") {
        return Value(
            std::make_shared<ListObject>(charsOf(text))
        );
    }

    if (name == "charAt") {
        double at = numberOf(args[0], "charAt");

        if (at < 0 || at >= static_cast<double>(text.size())) {
            return Value("");
        }

        return Value(std::string(1, text[static_cast<size_t>(at)]));
    }

    if (name == "toNumber") {
        return toNumber(self);
    }

    throw RuntimeError("string has no method '" + name + "'");
}

Value listMethod(
    const std::string& name,
    const Value& self,
    std::vector<Value>& args,
    VM& vm
) {
    auto list = std::static_pointer_cast<ListObject>(self.asObject());

    if (name == "len") {
        return Value(static_cast<double>(list->elements().size()));
    }

    if (name == "push") {
        for (const auto& item : args) {
            list->push(item);
        }

        return self;
    }

    if (name == "pop") {
        return list->pop();
    }

    if (name == "insert") {
        list->insert(
            static_cast<size_t>(wholeIndex(args[0], "insert").asNumber()),
            args[1]
        );

        return self;
    }

    if (name == "remove") {
        return Value(list->remove(args[0]));
    }

    if (name == "indexOf") {
        return Value(static_cast<double>(list->find(args[0])));
    }

    if (name == "contains") {
        return Value(list->contains(args[0]));
    }

    if (name == "join") {
        Value separator = args.empty() ? Value("") : args[0];
        return joinWith(separator, list->elements());
    }

    if (name == "slice") {
        const auto& items = list->elements();

        double size = static_cast<double>(items.size());

        double from = wholeIndex(args[0], "slice").asNumber();
        double to = args.size() > 1
            ? wholeIndex(args[1], "slice").asNumber()
            : size;

        if (from < 0) {
            from += size;
        }

        if (to < 0) {
            to += size;
        }

        from = std::max(0.0, std::min(from, size));
        to = std::max(from, std::min(to, size));

        std::vector<Value> out;

        for (double at = from; at < to; at++) {
            out.push_back(items[static_cast<size_t>(at)]);
        }

        return Value(
            std::make_shared<ListObject>(std::move(out))
        );
    }

    if (name == "reverse") {
        auto items = list->elements();
        std::reverse(items.begin(), items.end());

        return Value(
            std::make_shared<ListObject>(std::move(items))
        );
    }

    if (name == "at") {
        double size = static_cast<double>(list->elements().size());
        double position = wholeIndex(args[0], "at").asNumber();

        if (position < 0) {
            position += size;
        }

        if (
            position < 0 ||
            position >= size ||
            position != std::floor(position)
        ) {
            return Value();
        }

        return list->get(static_cast<size_t>(position));
    }

    if (name == "first") {
        return list->get(0);
    }

    if (name == "last") {
        return list->get(list->elements().size() - 1);
    }

    if (name == "map") {
        std::vector<Value> out;

        for (const auto& item : list->elements()) {
            std::vector<Value> call;
            call.push_back(item);

            out.push_back(callWith(vm, args[0], call));
        }

        return Value(
            std::make_shared<ListObject>(std::move(out))
        );
    }

    if (name == "filter") {
        std::vector<Value> out;

        for (const auto& item : list->elements()) {
            std::vector<Value> call;
            call.push_back(item);

            if (callWith(vm, args[0], call).isTruthy()) {
                out.push_back(item);
            }
        }

        return Value(
            std::make_shared<ListObject>(std::move(out))
        );
    }

    if (name == "reduce") {
        Value total = args[1];

        for (const auto& item : list->elements()) {
            std::vector<Value> call;
            call.push_back(total);
            call.push_back(item);

            total = callWith(vm, args[0], call);
        }

        return total;
    }

    if (name == "sort") {
        auto items = list->elements();

        if (args.empty()) {
            std::stable_sort(
                items.begin(),
                items.end(),
                [](const Value& a, const Value& b) {
                    if (a.isNumber() && b.isNumber()) {
                        return a.asNumber() < b.asNumber();
                    }

                    return a.toString() < b.toString();
                }
            );
        } else {
            Value comparator = args[0];

            std::stable_sort(
                items.begin(),
                items.end(),
                [&vm, &comparator](const Value& a, const Value& b) {
                    std::vector<Value> call;
                    call.push_back(a);
                    call.push_back(b);

                    return callWith(vm, comparator, call).isTruthy();
                }
            );
        }

        return Value(
            std::make_shared<ListObject>(std::move(items))
        );
    }

    if (name == "min" || name == "max") {
        const auto& items = list->elements();

        if (items.empty()) {
            throw RuntimeError("cannot use '" + name + "' on an empty list");
        }

        Value best = items.front();

        for (const auto& item : items) {
            if (item.isNumber() && best.isNumber()) {
                if (name == "min" ? item.asNumber() < best.asNumber() :
                    item.asNumber() > best.asNumber()) {
                    best = item;
                }
            } else if (name == "min" ?
                item.toString() < best.toString() :
                item.toString() > best.toString()) {
                best = item;
            }
        }

        return best;
    }

    if (name == "each") {
        for (const auto& item : list->elements()) {
            std::vector<Value> call;
            call.push_back(item);

            callWith(vm, args[0], call);
        }

        return Value();
    }

    if (name == "copy") {
        return Value(
            std::make_shared<ListObject>(list->elements())
        );
    }

    throw RuntimeError("list has no method '" + name + "'");
}

Value mapMethod(
    const std::string& name,
    const Value& self,
    std::vector<Value>& args
) {
    auto map = std::static_pointer_cast<MapObject>(self.asObject());

    if (name == "len" || name == "size") {
        return Value(static_cast<double>(map->size()));
    }

    if (name == "keys") {
        std::vector<Value> out;

        for (const auto& key : map->keys()) {
            out.push_back(Value(key));
        }

        return Value(
            std::make_shared<ListObject>(std::move(out))
        );
    }

    if (name == "values") {
        return Value(
            std::make_shared<ListObject>(map->values())
        );
    }

    if (name == "get") {
        return map->get(mapKeyOf(args[0]));
    }

    if (name == "set") {
        map->set(mapKeyOf(args[0]), args[1]);
        return self;
    }

    if (name == "remove") {
        return Value(map->remove(mapKeyOf(args[0])));
    }

    if (name == "contains" || name == "has") {
        return Value(map->contains(mapKeyOf(args[0])));
    }

    if (name == "clear") {
        map->clear();
        return Value();
    }

    throw RuntimeError("map has no method '" + name + "'");
}

Value dispatch(
    VM& vm,
    const std::string& name,
    const Value& self,
    std::vector<Value>& args
) {
    if (self.isString()) {
        return stringMethod(name, self, args);
    }

    if (self.isObject()) {
        auto type = self.asObject()->type();

        if (type == ObjectType::List) {
            return listMethod(name, self, args, vm);
        }

        if (type == ObjectType::Map) {
            return mapMethod(name, self, args);
        }
    }

    throw RuntimeError(
        self.typeName() + " has no method '" + name + "'"
    );
}

const std::vector<MethodEntry>& stringMethods() {
    static const std::vector<MethodEntry> table = {
        {"len", 0, 0},
        {"upper", 0, 0},
        {"lower", 0, 0},
        {"trim", 0, 0},
        {"split", 1, 1},
        {"contains", 1, 1},
        {"startsWith", 1, 1},
        {"endsWith", 1, 1},
        {"replace", 2, 2},
        {"indexOf", 1, 1},
        {"substring", 1, 2},
        {"repeat", 1, 1},
        {"chars", 0, 0},
        {"charAt", 1, 1},
        {"toNumber", 0, 0}
    };

    return table;
}

const std::vector<MethodEntry>& listMethods() {
    static const std::vector<MethodEntry> table = {
        {"len", 0, 0},
        {"push", 1, -1},
        {"pop", 0, 0},
        {"insert", 2, 2},
        {"remove", 1, 1},
        {"indexOf", 1, 1},
        {"contains", 1, 1},
        {"join", 0, 1},
        {"slice", 1, 2},
        {"reverse", 0, 0},
        {"at", 1, 1},
        {"first", 0, 0},
        {"last", 0, 0},
        {"map", 1, 1},
        {"filter", 1, 1},
        {"reduce", 2, 2},
        {"sort", 0, 1},
        {"each", 1, 1},
        {"min", 0, 0},
        {"max", 0, 0},
        {"copy", 0, 0}
    };

    return table;
}

const std::vector<MethodEntry>& mapMethods() {
    static const std::vector<MethodEntry> table = {
        {"len", 0, 0},
        {"size", 0, 0},
        {"keys", 0, 0},
        {"values", 0, 0},
        {"get", 1, 1},
        {"set", 2, 2},
        {"remove", 1, 1},
        {"contains", 1, 1},
        {"has", 1, 1},
        {"clear", 0, 0}
    };

    return table;
}

}

Value getProperty(
    const Value& target,
    const std::string& name,
    int line
) {
    const std::vector<MethodEntry>* table = nullptr;

    if (target.isString()) {
        table = &stringMethods();
    } else if (target.isObject()) {
        auto type = target.asObject()->type();

        if (type == ObjectType::List) {
            table = &listMethods();
        } else if (type == ObjectType::Map) {
            table = &mapMethods();
        }
    }

    if (table != nullptr) {
        for (const auto& entry : *table) {
            if (entry.name != name) {
                continue;
            }

            auto bound = function(
                entry.name,
                entry.minArgs,
                entry.maxArgs,
                [name, target](VM& vm, std::vector<Value>& args) {
                    return dispatch(vm, name, target, args);
                }
            );

            bound->bind(target);

            return Value(bound);
        }
    }

    if (target.isObject() &&
        target.asObject()->type() == ObjectType::Map) {
        auto map = std::static_pointer_cast<MapObject>(target.asObject());

        if (map->contains(name)) {
            return map->get(name);
        }

        return Value();
    }

    throw RuntimeError(
        target.typeName() + " has no method '" + name + "'",
        { line, 0 }
    );
}

void registerBuiltins(VM& vm) {
    define(vm, "print", 0, -1, printValues);

    define(vm, "len", 1, 1, [](VM&, std::vector<Value>& args) {
        return lengthOf(args[0]);
    });

    define(vm, "str", 1, 1, [](VM& machine, std::vector<Value>& args) {
        return machine.display(args[0]);
    });

    define(vm, "num", 1, 1, [](VM&, std::vector<Value>& args) {
        return toNumber(args[0]);
    });

    define(vm, "bool", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(args[0].isTruthy());
    });

    define(vm, "type", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(args[0].typeName());
    });

    define(vm, "input", 0, 1, [](VM&, std::vector<Value>& args) {
        if (!args.empty()) {
            std::cout << args[0].toString();
            std::cout.flush();
        }

        std::string line;

        if (!std::getline(std::cin, line)) {
            return Value("");
        }

        return Value(line);
    });

    define(vm, "abs", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(std::fabs(numberOf(args[0], "abs")));
    });

    define(vm, "floor", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(std::floor(numberOf(args[0], "floor")));
    });

    define(vm, "ceil", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(std::ceil(numberOf(args[0], "ceil")));
    });

    define(vm, "round", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(std::round(numberOf(args[0], "round")));
    });

    define(vm, "sqrt", 1, 1, [](VM&, std::vector<Value>& args) {
        double value = numberOf(args[0], "sqrt");

        if (value < 0) {
            throw RuntimeError("sqrt needs a number of zero or more");
        }

        return Value(std::sqrt(value));
    });

    define(vm, "pow", 2, 2, [](VM&, std::vector<Value>& args) {
        return Value(
            std::pow(
                numberOf(args[0], "pow"),
                numberOf(args[1], "pow")
            )
        );
    });

    define(vm, "min", 1, -1, [](VM&, std::vector<Value>& args) {
        double best = numberOf(args[0], "min");

        for (const auto& item : args) {
            best = std::min(best, numberOf(item, "min"));
        }

        return Value(best);
    });

    define(vm, "max", 1, -1, [](VM&, std::vector<Value>& args) {
        double best = numberOf(args[0], "max");

        for (const auto& item : args) {
            best = std::max(best, numberOf(item, "max"));
        }

        return Value(best);
    });

    define(vm, "ping", 1, 3, [](VM&, std::vector<Value>& args) {
        const std::string& host = stringOf(args[0], "ping");

        int timeout = args.size() > 1 ?
            static_cast<int>(numberOf(args[1], "ping")) : 500;

        if (timeout < 1) {
            throw RuntimeError("ping needs a timeout of one millisecond or more");
        }

        if (timeout > 60000) {
            throw RuntimeError("ping waits at most 60000 milliseconds");
        }

        int port = args.size() > 2 ?
            static_cast<int>(numberOf(args[2], "ping")) : 0;

        if (port < 0 || port > 65535) {
            throw RuntimeError("ping needs a port between 0 and 65535");
        }

        return Value(NovaNet::ping(host, timeout, port).reachable);
    });

    define(vm, "scan", 3, 6, [](VM&, std::vector<Value>& args) {
        const std::string& prefix = stringOf(args[0], "scan");

        int first = static_cast<int>(numberOf(args[1], "scan"));
        int last = static_cast<int>(numberOf(args[2], "scan"));

        int timeout = args.size() > 3 ?
            static_cast<int>(numberOf(args[3], "scan")) : 400;
        int port = args.size() > 4 ?
            static_cast<int>(numberOf(args[4], "scan")) : 0;
        int jobs = args.size() > 5 ?
            static_cast<int>(numberOf(args[5], "scan")) : 32;

        if (timeout < 1 || timeout > 60000) {
            throw RuntimeError(
                "scan needs a timeout between 1 and 60000 milliseconds"
            );
        }

        if (port < 0 || port > 65535) {
            throw RuntimeError("scan needs a port between 0 and 65535");
        }

        std::vector<NovaNet::Host> live;

        try {
            live = NovaNet::scan(prefix, first, last, timeout, port, jobs);
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }

        std::vector<Value> answers;

        for (const auto& host : live) {
            answers.push_back(Value(host.address));
        }

        return Value(std::make_shared<ListObject>(std::move(answers)));
    });

    define(vm, "scanDetail", 3, 6, [](VM&, std::vector<Value>& args) {
        const std::string& prefix = stringOf(args[0], "scanDetail");

        int first = static_cast<int>(numberOf(args[1], "scanDetail"));
        int last = static_cast<int>(numberOf(args[2], "scanDetail"));

        int timeout = args.size() > 3 ?
            static_cast<int>(numberOf(args[3], "scanDetail")) : 400;
        int port = args.size() > 4 ?
            static_cast<int>(numberOf(args[4], "scanDetail")) : 0;
        int jobs = args.size() > 5 ?
            static_cast<int>(numberOf(args[5], "scanDetail")) : 32;

        if (timeout < 1 || timeout > 60000) {
            throw RuntimeError(
                "scanDetail needs a timeout between 1 and 60000 milliseconds"
            );
        }

        if (port < 0 || port > 65535) {
            throw RuntimeError(
                "scanDetail needs a port between 0 and 65535"
            );
        }

        std::vector<NovaNet::Host> live;

        try {
            live = NovaNet::scan(prefix, first, last, timeout, port, jobs);
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }

        std::vector<Value> answers;

        for (const auto& host : live) {
            std::vector<std::pair<std::string, Value>> entries;
            entries.emplace_back("ip", Value(host.address));
            entries.emplace_back("port", Value(static_cast<double>(host.port)));

            answers.push_back(
                Value(
                    std::make_shared<MapObject>(std::move(entries))
                )
            );
        }

        return Value(std::make_shared<ListObject>(std::move(answers)));
    });

    define(vm, "lookup", 1, 1, [](VM&, std::vector<Value>& args) {
        const std::string& host = stringOf(args[0], "lookup");

        std::vector<Value> answers;

        for (const auto& address : NovaNet::resolve(host)) {
            answers.push_back(Value(address));
        }

        return Value(std::make_shared<ListObject>(std::move(answers)));
    });

    define(vm, "get", 1, 2, [](VM&, std::vector<Value>& args) {
        const std::string& url = stringOf(args[0], "get");

        int timeout = args.size() > 1 ?
            static_cast<int>(numberOf(args[1], "get")) : 5000;

        if (timeout < 1 || timeout > 120000) {
            throw RuntimeError(
                "get needs a timeout between 1 and 120000 milliseconds"
            );
        }

        try {
            return Value(NovaNet::get(url, timeout));
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }
    });

    define(vm, "readFile", 1, 1, [](VM&, std::vector<Value>& args) {
        try {
            return Value(NovaIo::read(stringOf(args[0], "readFile")));
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }
    });

    define(vm, "writeFile", 2, 2, [](VM&, std::vector<Value>& args) {
        const std::string& path = stringOf(args[0], "writeFile");

        try {
            NovaIo::write(path, args[1].toString());
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }

        return Value(true);
    });

    define(vm, "appendFile", 2, 2, [](VM&, std::vector<Value>& args) {
        const std::string& path = stringOf(args[0], "appendFile");

        try {
            NovaIo::append(path, args[1].toString());
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }

        return Value(true);
    });

    define(vm, "fileExists", 1, 1, [](VM&, std::vector<Value>& args) {
        return Value(NovaIo::exists(stringOf(args[0], "fileExists")));
    });

    define(vm, "removeFile", 1, 1, [](VM&, std::vector<Value>& args) {
        try {
            return Value(NovaIo::remove(stringOf(args[0], "removeFile")));
        } catch (const std::runtime_error& problem) {
            throw RuntimeError(problem.what());
        }
    });

    define(vm, "sum", 1, -1, sumOf);
    define(vm, "range", 1, 3, makeRange);

    define(vm, "random", 0, 2, [](VM&, std::vector<Value>& args) {
        if (args.empty()) {
            std::uniform_real_distribution<double> unit(0.0, 1.0);
            return Value(unit(engine()));
        }

        if (args.size() == 1) {
            double limit = numberOf(args[0], "random");

            if (limit < 1) {
                throw RuntimeError(
                    "random needs a limit of one or more"
                );
            }

            std::uniform_int_distribution<int> whole(
                1,
                static_cast<int>(limit)
            );

            return Value(static_cast<double>(whole(engine())));
        }

        double from = numberOf(args[0], "random");
        double to = numberOf(args[1], "random");

        if (to <= from) {
            throw RuntimeError(
                "random needs the last number to be bigger"
            );
        }

        std::uniform_real_distribution<double> unit(from, to);

        return Value(unit(engine()));
    });

    define(vm, "error", 1, 1, [](VM&, std::vector<Value>& args) -> Value {
        throw RuntimeError(args[0].toString());
    });

    define(vm, "assert", 1, 2, [](VM&, std::vector<Value>& args) -> Value {
        if (args[0].isTruthy()) {
            return Value(true);
        }

        throw RuntimeError(
            args.size() > 1
                ? args[1].toString()
                : std::string("assertion failed")
        );
    });
}

}
