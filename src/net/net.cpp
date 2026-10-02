#include "net.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace NovaNet {

namespace {

const int commonPorts[] = {80, 443, 22, 445, 3389, 8080};

std::string describe(int code) {
    return std::strerror(code);
}

int waitFor(int socketFd, int timeoutMs, bool wantWrite) {
    pollfd waiting{};
    waiting.fd = socketFd;
    waiting.events = wantWrite ? POLLOUT : POLLIN;

    int ready = ::poll(&waiting, 1, timeoutMs);

    if (ready < 0) {
        return -1;
    }

    return ready > 0 ? 0 : 1;
}

int connectTo(
    const std::string& host,
    int port,
    int timeoutMs,
    std::string& detail,
    bool& refused
) {
    refused = false;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;

    const std::string service = std::to_string(port);

    int found = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &results);

    if (found != 0) {
        detail = "unknown host '" + host + "'";
        return -1;
    }

    int socketFd = -1;
    detail = "no route to '" + host + "'";

    for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        socketFd = ::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);

        if (socketFd < 0) {
            detail = describe(errno);
            continue;
        }

        int flags = ::fcntl(socketFd, F_GETFL, 0);
        ::fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);

        int started = ::connect(
            socketFd,
            entry->ai_addr,
            entry->ai_addrlen
        );

        if (started < 0 && errno != EINPROGRESS) {
            detail = describe(errno);
            ::close(socketFd);
            socketFd = -1;
            continue;
        }

        if (started < 0) {
            int state = waitFor(socketFd, timeoutMs, true);

            if (state != 0) {
                detail = state < 0 ? describe(errno) : "no answer";
                ::close(socketFd);
                socketFd = -1;
                continue;
            }

            int problem = 0;
            socklen_t size = sizeof(problem);

            ::getsockopt(
                socketFd,
                SOL_SOCKET,
                SO_ERROR,
                &problem,
                &size
            );

            if (problem != 0) {
                refused = problem == ECONNREFUSED;
                detail = describe(problem);
                ::close(socketFd);
                socketFd = -1;
                continue;
            }
        }

        detail.clear();
        break;
    }

    ::freeaddrinfo(results);

    return socketFd;
}

std::string lowercase(const std::string& text) {
    std::string result = text;

    for (auto& c : result) {
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))
        );
    }

    return result;
}

} 

bool looksLikeAddress(const std::string& text) {
    in_addr address{};

    return ::inet_pton(AF_INET, text.c_str(), &address) == 1;
}

std::vector<std::string> resolve(const std::string& host) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;

    int found = ::getaddrinfo(host.c_str(), nullptr, &hints, &results);

    if (found != 0) {
        return {};
    }

    std::vector<std::string> answers;

    for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        char text[INET_ADDRSTRLEN] = {};
        auto* address = reinterpret_cast<sockaddr_in*>(entry->ai_addr);

        if (::inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text)) != nullptr) {
            std::string entryText = text;

            if (std::find(answers.begin(), answers.end(), entryText) ==
                answers.end()) {
                answers.push_back(entryText);
            }
        }
    }

    ::freeaddrinfo(results);

    return answers;
}

PingResult ping(
    const std::string& host,
    int timeoutMs,
    int port
) {
    PingResult result;

    std::vector<int> ports;

    if (port > 0) {
        ports.push_back(port);
    } else {
        for (int value : commonPorts) {
            ports.push_back(value);
        }
    }

    for (int candidate : ports) {
        std::string detail;
        bool refused = false;

        int socketFd = connectTo(
            host,
            candidate,
            timeoutMs,
            detail,
            refused
        );

        if (socketFd >= 0) {
            ::close(socketFd);

            result.reachable = true;
            result.port = candidate;
            return result;
        }

        if (detail.find("unknown host") == 0) {
            result.detail = detail;
            return result;
        }

        if (!refused) {
            result.detail = detail;
            return result;
        }
    }

    result.detail = "nothing answered on the ports tried";

    return result;
}

std::vector<Host> scan(
    const std::string& prefix,
    int first,
    int last,
    int timeoutMs,
    int port,
    int jobs
) {
    if (first < 0 || last > 255 || first > last) {
        throw std::runtime_error(
            "scan needs a host range between 0 and 255"
        );
    }

    for (char c : prefix) {
        if (c != '.' && (c < '0' || c > '9')) {
            throw std::runtime_error(
                "scan needs a prefix such as '192.168.0.'"
            );
        }
    }

    if (jobs < 1) {
        jobs = 1;
    }

    if (jobs > 128) {
        jobs = 128;
    }

    std::vector<Host> hosts;

    for (int host = first; host <= last; host++) {
        hosts.push_back({ prefix + std::to_string(host), false, 0 });
    }

    std::atomic<size_t> next{0};

    auto worker = [&]() {
        while (true) {
            size_t index = next.fetch_add(1);

            if (index >= hosts.size()) {
                return;
            }

            Host& entry = hosts[index];
            PingResult result = ping(entry.address, timeoutMs, port);

            entry.reachable = result.reachable;
            entry.port = result.port;
        }
    };

    std::vector<std::thread> pool;

    for (int i = 0; i < jobs && i < static_cast<int>(hosts.size()); i++) {
        pool.emplace_back(worker);
    }

    for (auto& thread : pool) {
        thread.join();
    }

    std::vector<Host> live;

    for (const auto& entry : hosts) {
        if (entry.reachable) {
            live.push_back(entry);
        }
    }

    return live;
}

std::string get(const std::string& url, int timeoutMs) {
    std::string rest = url;

    std::string scheme;

    size_t mark = rest.find("://");

    if (mark != std::string::npos) {
        scheme = lowercase(rest.substr(0, mark));
        rest = rest.substr(mark + 3);
    }

    if (scheme == "https") {
        throw std::runtime_error(
            "get does not speak https yet, use http://"
        );
    }

    if (!scheme.empty() && scheme != "http") {
        throw std::runtime_error(
            "get understands http:// addresses only"
        );
    }

    std::string host = rest;
    std::string path = "/";
    int port = 80;

    size_t slash = rest.find('/');

    if (slash != std::string::npos) {
        host = rest.substr(0, slash);
        path = rest.substr(slash);
    }

    size_t colon = host.find(':');

    if (colon != std::string::npos) {
        port = std::stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }

    if (host.empty()) {
        throw std::runtime_error("get needs a host name");
    }

    std::string detail;
    bool refused = false;

    int socketFd = connectTo(host, port, timeoutMs, detail, refused);

    if (socketFd < 0) {
        throw std::runtime_error(
            "cannot reach " + host + ": " + detail
        );
    }

    std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: nova/0.1\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n";

    size_t sent = 0;

    while (sent < request.size()) {
        ssize_t written = ::send(
            socketFd,
            request.data() + sent,
            request.size() - sent,
            MSG_NOSIGNAL
        );

        if (written <= 0) {
            ::close(socketFd);

            throw std::runtime_error("the request could not be sent");
        }

        sent += static_cast<size_t>(written);
    }

    std::string answer;

    char buffer[4096];

    while (answer.size() < 4u * 1024u * 1024u) {
        pollfd waiting{};
        waiting.fd = socketFd;
        waiting.events = POLLIN;

        int ready = ::poll(&waiting, 1, timeoutMs);

        if (ready <= 0) {
            break;
        }

        ssize_t taken = ::recv(socketFd, buffer, sizeof(buffer), 0);

        if (taken <= 0) {
            break;
        }

        answer.append(buffer, static_cast<size_t>(taken));
    }

    ::close(socketFd);

    if (answer.empty()) {
        throw std::runtime_error(
            "the server closed the connection without an answer"
        );
    }

    size_t end = answer.find("\r\n\r\n");

    if (end == std::string::npos) {
        throw std::runtime_error("the answer has no header end");
    }

    std::string head = answer.substr(0, end);

    if (head.find(" 200") == std::string::npos &&
        head.find(" 30") == std::string::npos) {
        size_t space = head.find(' ');

        std::string code = space != std::string::npos ?
            head.substr(space + 1, 3) : "?";

        throw std::runtime_error("the server answered " + code);
    }

    return answer.substr(end + 4);
}

}