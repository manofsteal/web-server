#pragma once
#include "listener.hpp"
#include "pollable.hpp"
#include "pollable_pool.hpp"
#include "socket.hpp"
#include "timer.hpp"
#include <cstdint>
#include <map>

struct PollablePoolManager {
  void cleanup() {
    timers.items.clear();
    sockets.items.clear();
    listeners.items.clear();
  }

  PollableIDManager id_manager = PollableIDManager{};

  PollablePool<Timer> timers = PollablePool<Timer>{};
  PollablePool<Socket> sockets = PollablePool<Socket>{};
  PollablePool<Listener> listeners = PollablePool<Listener>{};
};