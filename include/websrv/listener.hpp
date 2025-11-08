#pragma once

#include "websrv/pollable.hpp"
#include "websrv/socket.hpp"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace websrv {

struct Listener : Pollable {
  using AcceptCallback = std::function<void(Socket *)>;
  AcceptCallback onAccept = [](Socket *) {};
  uint16_t port = 0;

  Listener();

  bool start(uint16_t port);

  void stop();
};

} // namespace websrv