#pragma once

#include <string>

namespace NovaIo {

std::string read(const std::string& path);

void write(const std::string& path, const std::string& text);

void append(const std::string& path, const std::string& text);

bool exists(const std::string& path);

bool remove(const std::string& path);

}