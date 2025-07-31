#pragma once
#include "executor.hpp"
#include "listener.hpp"
#include "pollable.hpp"
#include "pollable_pool_manager.hpp"
#include "socket.hpp"
#include <errno.h>
#include <functional>
#include <map>
#include <poll.h>
#include <signal.h> // For sigaction and siginfo_t
#include <stdexcept>
#include <vector>
#include <chrono>

struct Poller {
  struct PollEntry {
    Pollable *pollable = nullptr;
    short events = 0; // poll events (POLLIN, POLLOUT, etc.)
  };

  // Timer structures for poll-based timing
  using TimerID = uint32_t;
  using TimerCallback = std::function<void()>;
  
  struct TimerEntry {
    TimerID id;
    std::chrono::steady_clock::time_point expiry_time;
    uint32_t interval_ms;
    TimerCallback callback;
    bool is_interval;
    bool active;
  };

  TimerID next_timer_id = 1;
  std::map<TimerID, TimerEntry> timers;

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
  Listener *createListener();
  void removePollable(PollableID id);

  void start();
  void stop();

  // Method to enable POLLOUT for a socket (thread-safe)
  void enablePollout(PollableID socket_id);

  // Timer methods
  TimerID setTimeout(uint32_t ms, TimerCallback callback);
  TimerID setInterval(uint32_t ms, TimerCallback callback);
  void clearTimeout(TimerID timer_id);
  void clearInterval(TimerID timer_id);

protected:
  void addPollable(Pollable *pollable);

  // Helper method to update poll events safely
  void updatePollEvents();

  // Timer helper methods
  int calculatePollTimeout();
  void processExpiredTimers();
};
