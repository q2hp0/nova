#include "file_io.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace NovaIo {

namespace {

std::runtime_error failure(const std::string& path, const char* action) {
    return std::runtime_error(
        std::string(action) + " '" + path + "': " + std::strerror(errno)
    );
}

std::ifstream openForReading(const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) {
        throw failure(path, "cannot read");
    }

    return file;
}

void openForWriting(
    const std::string& path,
    std::ofstream& file,
    bool appending
) {
    std::ios_base::openmode mode = std::ios_base::binary |
        (appending ? std::ios_base::app : std::ios_base::trunc);

    file.open(path, mode);

    if (!file.is_open()) {
        throw failure(path, "cannot write");
    }
}

} 

std::string read(const std::string& path) {
    std::ifstream file = openForReading(path);

    std::ostringstream buffer;

    buffer << file.rdbuf();

    if (file.bad()) {
        throw failure(path, "cannot read");
    }

    return buffer.str();
}

void write(const std::string& path, const std::string& text) {
    std::ofstream file;

    openForWriting(path, file, false);

    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();

    if (!file.good()) {
        throw failure(path, "cannot write");
    }
}

void append(const std::string& path, const std::string& text) {
    std::ofstream file;

    openForWriting(path, file, true);

    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();

    if (!file.good()) {
        throw failure(path, "cannot write");
    }
}

bool exists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    return file.good();
}

bool remove(const std::string& path) {
    if (::remove(path.c_str()) != 0) {
        if (errno == ENOENT) {
            return false;
        }

        throw failure(path, "cannot remove");
    }

    return true;
}

}