#include "errors.h"

std::string formatAt(
    const SourceLocation& location,
    const std::string& message
) {
    if (location.line == 0) {
        return message;
    }

    return "line " +
        std::to_string(location.line) +
        ":" +
        std::to_string(location.column) +
        " " +
        message;
}

NovaError::NovaError(
    const std::string& message,
    SourceLocation location
)
    : std::runtime_error(formatAt(location, message)),
      errorLocation(location) {
}

SourceLocation NovaError::location() const {
    return errorLocation;
}
