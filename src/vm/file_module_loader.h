#ifndef NOVA_FILE_MODULE_LOADER_H
#define NOVA_FILE_MODULE_LOADER_H

#include "vm.h"

#include <fstream>
#include <sstream>
#include <string>

class FileModuleLoader : public ModuleLoader {
public:
    explicit FileModuleLoader(std::string root)
        : root(std::move(root)) {
    }

    std::string load(const std::string& path) override {
        std::string resolved = resolve(path);

        std::ifstream file(resolved);

        if (!file.is_open()) {
            return "";
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return buffer.str();
    }

    std::string resolve(const std::string& path) const {
        if (!path.empty() && path[0] == '/') {
            return path;
        }

        if (root.empty()) {
            return path;
        }

        if (root.back() == '/') {
            return root + path;
        }

        return root + "/" + path;
    }

private:
    std::string root;
};


#endif
