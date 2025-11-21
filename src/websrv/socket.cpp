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
  // Buffers are created on-demand
}

Socket::~Socket() {
  // Release all pending read buffers
  for (Buffer* buf : pending_read_buffers) {
    if (buf) {
      Pollable::releaseBuffer(buf);
    }
  }
  pending_read_buffers.clear();
  
  // Release all pending write buffers
  for (Buffer* buf : pending_write_buffers) {
    if (buf) {
      Pollable::releaseBuffer(buf);
    }
  }
  pending_write_buffers.clear();
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

void Socket::write(Buffer* buffer) {
  if (!buffer) return;
  
  // Add buffer to pending write queue
  pending_write_buffers.push_back(buffer);
  
  // Note: POLLOUT is managed by SocketManager, not here.
  // This keeps Socket independent of Poller and follows the manager pattern.
  // SocketManager will automatically enable POLLOUT for sockets with pending writes.
}

bool Socket::handleRead() {
    if (file_descriptor < 0) return false;
    
    // Get or create current read buffer
    Buffer* buffer = nullptr;
    if (pending_read_buffers.empty() || pending_read_buffers.back()->size() >= 4096) {
        // Create new buffer if none exists or current is getting full
        buffer = Pollable::getBuffer();
        pending_read_buffers.push_back(buffer);
    } else {
        buffer = pending_read_buffers.back();
    }
    
    char temp[4096];
    ssize_t bytes_read = ::read(file_descriptor, temp, sizeof(temp));

    if (bytes_read > 0) {
        buffer->append(temp, bytes_read);
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
    if (file_descriptor < 0 || pending_write_buffers.empty()) return false;
    
    // Get the first pending buffer
    Buffer* buffer = pending_write_buffers.front();
    if (!buffer || buffer->size() == 0) {
        // Empty buffer, remove and try next
        pending_write_buffers.erase(pending_write_buffers.begin());
        Pollable::releaseBuffer(buffer);
        return !pending_write_buffers.empty();
    }
    
    // Extract data from Buffer to write
    std::vector<char> temp;
    size_t size = buffer->size();
    temp.reserve(size);
    for(size_t i = 0; i < size; ++i) {
        temp.push_back(buffer->getAt(i));
    }
    
    ssize_t bytes_written = ::write(file_descriptor, temp.data(), temp.size());
    
    if (bytes_written > 0) {
        // For now, assume all written (simplification)
        // Remove buffer from queue and release it
        pending_write_buffers.erase(pending_write_buffers.begin());
        Pollable::releaseBuffer(buffer);
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

std::vector<Buffer*> Socket::read() {
    // Transfer ownership of all buffers to caller
    std::vector<Buffer*> result = std::move(pending_read_buffers);
    pending_read_buffers.clear();
    return result;
}

Buffer* Socket::getReadBuffer() {
    // Return the last pending read buffer if it exists
    // Otherwise create a new one
    if (!pending_read_buffers.empty()) {
        return pending_read_buffers.back();
    }
    
    // Create new buffer and add to queue
    Buffer* buf = Pollable::getBuffer();
    pending_read_buffers.push_back(buf);
    return buf;
}

void Socket::clearReadBuffer() {
    // Release all read buffers
    for (Buffer* buf : pending_read_buffers) {
        if (buf) {
            Pollable::releaseBuffer(buf);
        }
    }
    pending_read_buffers.clear();
}

Buffer* Socket::getWriteBuffer() {
    // Return the last pending buffer if it exists and is not empty
    // Otherwise create a new buffer
    if (!pending_write_buffers.empty()) {
        return pending_write_buffers.back();
    }
    
    // Create new buffer and add to queue
    Buffer* buf = Pollable::getBuffer();
    pending_write_buffers.push_back(buf);
    return buf;
}

} // namespace websrv
