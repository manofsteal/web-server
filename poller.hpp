#pragma once
#include "executor.hpp"
#include "listener.hpp"
#include "pollable.hpp"
#include "pollable_pool_manager.hpp"
#include "socket.hpp"
#include "timer.hpp"
#include <errno.h>
#include <functional>
#include <map>
#include <poll.h>
#include <signal.h> // For sigaction and siginfo_t
#include <stdexcept>
#include <vector>

struct Poller {
  struct PollEntry {
    Pollable *pollable = nullptr;
    short events = 0; // poll events (POLLIN, POLLOUT, etc.)
  };

  PollablePoolManager poolManager = PollablePoolManager{};
  std::vector<pollfd> pollFds = {};
  std::map<PollableID, PollEntry> pollEntries = {};
  bool running = false;

  // Tracking of sockets that need POLLOUT enabled
  std::map<PollableID, bool> pollout_pending = {};

  // Executor for handling callbacks in separate threads
  Executor executor{};

  // Constructor - initialize executor with default thread count
  Poller(size_t executorThreads = std::thread::hardware_concurrency())
      : executor(executorThreads) {}

  // Factory methods
  Socket *createSocket();
  Timer *createTimer();
  Listener *createListener();
  void removePollable(PollableID id);

  void start();
  void stop();

  // Method to enable POLLOUT for a socket (thread-safe)
  void enablePollout(PollableID socket_id);

protected:
  void addPollable(Pollable *pollable);

  // Helper method to update poll events safely
  void updatePollEvents();
};
