#include "websrv/socket.hpp"
#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <vector>

namespace websrv {

Socket::Socket() : Pollable() {
  type = PollableType::SOCKET;
}

bool Socket::start(const std::string &host, uint16_t port) {
  file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (file_descriptor < 0) {
    return false;
  }

  // Set non-blocking
  int flags = fcntl(file_descriptor, F_GETFL, 0);
  if (flags != -1) {
    fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK);
  }

  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo *result = nullptr;
  std::string port_str = std::to_string(port);
  int res = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (res != 0 || !result) {
    ::close(file_descriptor);
    LOG_ERROR("Failed to resolve address ", host);
    return false;
  }

  bool connected = false;
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    if (connect(file_descriptor, rp->ai_addr, rp->ai_addrlen) == 0) {
      connected = true;
      break;
    } else {
        if (errno == EINPROGRESS) {
            connected = true; // Connecting in background
            break;
        }
    }
  }
  freeaddrinfo(result);

  if (!connected) {
    ::close(file_descriptor);
    LOG_ERROR("Failed to connect to ", host, ":", port);
    return false;
  }

  remote_addr = host;
  remote_port = port;
  return true;
}

void Socket::write(const std::string &data) {
  bool was_empty = write_buffer.size() == 0;
  write_buffer.append(data.data(), data.size());

  // Enable POLLOUT if buffer was empty
  if (was_empty && poller) {
    poller->enablePollout(id);
  }
}

bool Socket::handleRead() {
    if (file_descriptor < 0) return false;
    
    char buffer[4096];
    ssize_t bytes_read = ::read(file_descriptor, buffer, sizeof(buffer));

    if (bytes_read > 0) {
        read_buffer.insert(read_buffer.end(), buffer, buffer + bytes_read);
        return true;
    } else if (bytes_read == 0) {
        // EOF - connection closed
        return false;
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return false; // Error
        }
        return false; // Would block
    }
}

bool Socket::handleWrite() {
    if (file_descriptor < 0 || write_buffer.size() == 0) return false;
    
    // Extract data from Buffer to write
    std::vector<char> temp;
    size_t size = write_buffer.size();
    temp.reserve(size);
    for(size_t i = 0; i < size; ++i) {
        temp.push_back(write_buffer.getAt(i));
    }
    
    ssize_t bytes_written = ::write(file_descriptor, temp.data(), temp.size());
    
    if (bytes_written > 0) {
        write_buffer.clear(); // Simplification: assume all written
        return true;
    }
    return false;
}

bool Socket::handleError(short revents) {
    if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return true;
    }
    return false;
}

BufferView Socket::receive() {
    if (read_buffer.empty()) {
        return BufferView{};
    }
    return BufferView{read_buffer.data(), read_buffer.size()};
}

void Socket::clearReadBuffer() {
    read_buffer.clear();
}

} // namespace websrv
