#pragma once

#include <string>
#include <vector>

namespace NovaNet {

struct PingResult {
    bool reachable = false;
    int port = 0;
    std::string detail;
};

struct Host {
    std::string address;
    bool reachable = false;
    int port = 0;
};

bool looksLikeAddress(const std::string& text);

std::vector<std::string> resolve(const std::string& host);

PingResult ping(const std::string& host, int timeoutMs, int port);

std::vector<Host> scan(
    const std::string& prefix,
    int first,
    int last,
    int timeoutMs,
    int port,
    int jobs
);

std::string get(const std::string& url, int timeoutMs);

}