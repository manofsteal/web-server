#pragma once
#include "websrv/pollable.hpp"
#include "websrv/pollable_pool_manager.hpp"
#include "websrv/socket.hpp"
#include "websrv/listener.hpp"
#include "websrv/steady_clock.hpp"
#include <chrono>
#include <errno.h>
#include <map>
#include <poll.h>
#include <vector>

namespace websrv {

struct PollerEvent {
  PollableID id;
  short revents; // POLLIN, POLLOUT, POLLERR, etc.
};

struct Poller {
  struct PollEntry {
    Pollable *pollable = nullptr;
    short events = 0; // poll events (POLLIN, POLLOUT, etc.)
  };

  // Timer structures for poll-based timing
  using TimerID = uint32_t;

  struct TimerEntry {
    TimerID id;
    SteadyClock::TimePoint expiry;
    uint64_t interval; // 0 if one-shot
    bool repeat;
    bool expired;
  };

  TimerID next_timer_id = 1;
  std::map<TimerID, TimerEntry> timers;

  PollablePoolManager poolManager = PollablePoolManager{};
  std::vector<pollfd> pollFds = {};
  std::map<PollableID, PollEntry> pollEntries = {};
  
  // Tracking of sockets that need POLLOUT enabled
  std::map<PollableID, bool> pollout_pending = {};

  // Notification pipe for breaking poll() calls
  int notification_pipe[2] = {-1, -1};

  // Constructor
  Poller();

  // Destructor
  ~Poller();

  // Factory methods
  Socket *createSocket();
  Listener *createListener();
  void removePollable(PollableID id);

  // Game loop method: returns list of events
  std::vector<PollerEvent> poll(int timeout_ms);

  // Method to enable POLLOUT for a socket
  void enablePollout(PollableID socket_id);
  void disablePollout(PollableID socket_id);

  // Notification methods for breaking poll() calls
  void notify(); // Wake up poll() call
  
  // Timer management
  TimerID createTimer(uint64_t timeout_ms, bool repeat);
  bool isTimerExpired(TimerID id);
  void resetTimer(TimerID id);
  void destroyTimer(TimerID id);

  // Lookup
  Pollable* getPollable(PollableID id);

protected:
  void addPollable(Pollable *pollable);

  // Helper method to update poll events safely
  void updatePollEvents();

  // Timer helper methods
  void processExpiredTimers();

  // Pipe helper methods
  bool createNotificationPipe();
  void closeNotificationPipe();
  bool hasNotificationPipe() const;
  void drainNotificationPipe();
};

} // namespace websrv
