#pragma once

#include "websrv/pollable.hpp"
#include "websrv/socket.hpp"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace websrv {

struct Listener : Pollable {
  Listener();

  bool start(uint16_t port);
  void stop();
  
  // Process POLLIN event - returns new socket if accepted, nullptr otherwise
  Socket* handleAccept(Poller& poller);

  friend class ListenerManager;
};

} // namespace websrv