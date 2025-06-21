#include "socket.hpp"
#include "poller.hpp"

Socket::Socket() : Pollable() {
  type = PollableType::SOCKET;

  onEvent = [this](short revents) {
    // poller->executor.submit([this, revents]() {
    if (file_descriptor >= 0) {
      if (revents & POLLIN) {
        char buffer[1024];
        ssize_t bytes_read = read(file_descriptor, buffer, sizeof(buffer));
        if (bytes_read > 0) {
          read_buffer.append(buffer, bytes_read);
          if (onData) {
            onData(*this, read_buffer);
          }
        }
      }
      if ((revents & POLLOUT) && write_buffer.size() > 0) {
        std::vector<char> temp_buffer;
        temp_buffer.reserve(std::min(write_buffer.size(), size_t(1024)));
        for (size_t i = 0; i < temp_buffer.capacity(); ++i) {
          temp_buffer.push_back(write_buffer.getAt(i));
        }
        ssize_t bytes_written =
            ::write(file_descriptor, temp_buffer.data(), temp_buffer.size());
        if (bytes_written > 0) {
          write_buffer.clear();
        }
      }
    }
    // });
  };
}

bool Socket::start(const std::string &host, uint16_t port) {
  file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (file_descriptor < 0) {
    return false;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
    close(file_descriptor);
    return false;
  }

  if (connect(file_descriptor, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    close(file_descriptor);
    return false;
  }

  remote_addr = host;
  remote_port = port;
  return true;
}

void Socket::write(const Buffer &data) {
  bool was_empty = write_buffer.size() == 0;
  // Copy data from Buffer to write_buffer
  for (size_t i = 0; i < data.size(); ++i) {
    char byte = data.getAt(i);
    write_buffer.append(&byte, 1);
  }

  // Enable POLLOUT if buffer was empty (so we weren't monitoring for write
  // events)
  if (was_empty && poller) {
    poller->enableSocketPollout(id);
  }
}

void Socket::write(const std::string &data) {
  bool was_empty = write_buffer.size() == 0;
  write_buffer.append(data.data(), data.size());

  // Enable POLLOUT if buffer was empty (so we weren't monitoring for write
  // events)
  if (was_empty && poller) {
    poller->enableSocketPollout(id);
  }
}
