#pragma once
#include "executor.hpp"
#include "listener.hpp"
#include "pollable.hpp"
#include "pollable_pool_manager.hpp"
#include "socket.hpp"
#include <chrono>
#include <errno.h>
#include <functional>
#include <map>
#include <poll.h>
#include <signal.h> // For sigaction and siginfo_t
#include <stdexcept>
#include <thread>
#include <unistd.h> // For pipe() and close()
#include <vector>

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

  // Maximum timeout for poll() calls (in milliseconds, -1 means no limit)
  int max_poll_timeout_ms = 1000;

  // Current poll timeout being used (for optimization)
  int current_poll_timeout_ms = 1000;

  PollablePoolManager poolManager = PollablePoolManager{};
  std::vector<pollfd> pollFds = {};
  std::map<PollableID, PollEntry> pollEntries = {};
  bool running = false;

  // Tracking of sockets that need POLLOUT enabled
  std::map<PollableID, bool> pollout_pending = {};

  // Executor for handling callbacks in separate threads
  Executor executor{};

  // Notification pipe for breaking poll() calls
  int notification_pipe[2] = {-1, -1};

  // Thread ID of the poller thread
  std::thread::id poller_thread_id;

  // Constructor - initialize executor with default thread count
  Poller(size_t executorThreads = std::thread::hardware_concurrency())
      : executor(executorThreads) {}

  // Destructor
  ~Poller() = default;

  // Factory methods
  Socket *createSocket();
  Listener *createListener();
  void removePollable(PollableID id);

  void start();
  void stop();

  // Method to enable POLLOUT for a socket (thread-safe)
  void enablePollout(PollableID socket_id);

  // Notification methods for breaking poll() calls
  void notify(); // Wake up poll() call from another thread
  void
  setMaxPollTimeout(int max_timeout_ms); // Set maximum timeout for poll() calls

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

  // Pipe helper methods
  bool createNotificationPipe();
  void closeNotificationPipe();
  bool hasNotificationPipe() const;
  void drainNotificationPipe();
};
