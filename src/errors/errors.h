#pragma once

#include "../runtime/value.h"

#include <stdexcept>
#include <string>

struct SourceLocation {
    int line = 0;
    int column = 0;
};

std::string formatAt(
    const SourceLocation& location,
    const std::string& message
);

class NovaError : public std::runtime_error {
public:
    explicit NovaError(
        const std::string& message,
        SourceLocation location = {}
    );

    SourceLocation location() const;

private:
    SourceLocation errorLocation;
};

class LexerError : public NovaError {
public:
    using NovaError::NovaError;
};

class ParserError : public NovaError {
public:
    using NovaError::NovaError;
};

class CompileError : public NovaError {
public:
    using NovaError::NovaError;
};

class RuntimeError : public NovaError {
public:
    using NovaError::NovaError;
};

class NovaThrow {
public:
    NovaThrow(Value value, int line)
        : thrown(std::move(value)), thrownLine(line) {
    }

    const Value& value() const {
        return thrown;
    }

    int line() const {
        return thrownLine;
    }

private:
    Value thrown;
    int thrownLine;
};
