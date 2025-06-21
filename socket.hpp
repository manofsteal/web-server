#pragma once

#include "buffer.hpp"
#include "pollable.hpp"
#include <arpa/inet.h>
#include <cstring> // for strerror
#include <errno.h> // for errno
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Forward declaration to avoid circular dependency
class Poller;

struct Socket : Pollable {
  Buffer read_buffer = Buffer{};
  Buffer write_buffer = Buffer{};

  std::string remote_addr = "";
  uint16_t remote_port = 0;

  using Callback = std::function<void(Socket &, const Buffer &)>;
  Callback onData = [](Socket &, const Buffer &) {};

  Socket();

  bool start(const std::string &host, uint16_t port);

  void write(const Buffer &data);
  void write(const std::string &data);
};