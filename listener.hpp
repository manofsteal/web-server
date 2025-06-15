#pragma once

#include "pollable.hpp"
#include "socket.hpp"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

struct Listener : Pollable {
  using AcceptCallback = std::function<void(Socket *)>;
  AcceptCallback on_accept;
  uint16_t port;

  // Constructor to replace init()
  Listener() : Pollable(PollableType::LISTENER), port(0) {}

  void setOnAccept(AcceptCallback cb) { on_accept = std::move(cb); }

  bool listen(uint16_t port) {
    this->port = port;
    file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (file_descriptor < 0)
      return false;

    int opt = 1;
    if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt)) < 0) {
      close(file_descriptor);
      return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(file_descriptor, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      close(file_descriptor);
      return false;
    }

    if (::listen(file_descriptor, SOMAXCONN) < 0) {
      close(file_descriptor);
      return false;
    }

    return true;
  }
};