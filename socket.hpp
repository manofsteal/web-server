#pragma once

#include "pollable.hpp"
#include <vector>
#include <string>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct Socket : Pollable {
    std::vector<char> read_buffer;
    std::vector<char> write_buffer;
    using Callback = std::function<void(Socket&, const std::vector<char>&)>;
    Callback on_data;
    std::string remote_addr;
    uint16_t remote_port = 0;

    bool init() {
        Pollable::init(PollableType::SOCKET);
        remote_port = 0;
        return true;
    }
    
    void setOnData(Callback cb) {
        on_data = std::move(cb);
    }

    bool connect(const std::string& host, uint16_t port) {
        file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
        if (file_descriptor < 0) return false;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::connect(file_descriptor, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(file_descriptor);
            file_descriptor = -1;
            return false;
        }

        remote_addr = host;
        remote_port = port;
        return true;
    }

    void write(const std::vector<char>& data) {
        write_buffer.insert(write_buffer.end(), data.begin(), data.end());
    }

    void write(const std::string& data) {
        write_buffer.insert(write_buffer.end(), data.begin(), data.end());
    }
}; 