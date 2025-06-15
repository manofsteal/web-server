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
    Pollable *pollable;
    short events; // poll events (POLLIN, POLLOUT, etc.)
    std::function<void()> callback;

    // Constructor to replace init()
    PollEntry(Pollable *p, short ev, std::function<void()> cb)
        : pollable(p), events(ev), callback(std::move(cb)) {}

    // Default constructor for map usage
    PollEntry() : pollable(nullptr), events(0) {}

    void cleanup() {
      pollable = nullptr;
      callback = nullptr;
      events = 0;
    }
  };

  PollablePoolManager poolManager;
  std::vector<pollfd> pollFds;
  std::map<PollableID, PollEntry> pollEntries;
  bool running = false;

  // Executor for handling callbacks in separate threads
  Executor executor;

  // Constructor - initialize executor with default thread count
  Poller(size_t executorThreads = std::thread::hardware_concurrency())
      : executor(executorThreads) {}

  // Factory methods
  Socket *createSocket();
  Timer *createTimer();
  Listener *createListener();

  void remove(PollableID id);
  void run();
  void stop();

  void cleanup();

protected:
  void addSocket(Socket *socket);
  void addTimer(Timer *timer);
  void addListener(Listener *listener);
};
