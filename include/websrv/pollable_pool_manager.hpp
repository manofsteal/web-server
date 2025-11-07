#pragma once
#include "websrv/listener.hpp"
#include "websrv/pollable.hpp"
#include "websrv/pollable_pool.hpp"
#include "websrv/socket.hpp"
#include <cstdint>
#include <map>

struct PollablePoolManager {
  void stop() {}

  PollableIDManager id_manager = PollableIDManager{};

  PollablePool<Socket> sockets = PollablePool<Socket>{};
  PollablePool<Listener> listeners = PollablePool<Listener>{};
};