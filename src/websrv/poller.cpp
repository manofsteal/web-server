#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

namespace websrv {

Poller::Poller() {
  createNotificationPipe();
}

Poller::~Poller() {
  closeNotificationPipe();
}

// Factory methods
Listener *Poller::createListener() {
  Listener *listener = poolManager.listeners.create(&poolManager.id_manager);
  addPollable(listener);
  return listener;
}

Socket *Poller::createSocket() {
  Socket *socket = poolManager.sockets.create(&poolManager.id_manager);
  addPollable(socket);
  return socket;
}

void Poller::addPollable(Pollable *pollable) {
  if (!pollable)
    return;
  pollable->poller = this;
  pollEntries[pollable->id] = PollEntry{pollable, POLLIN};
}

void Poller::removePollable(PollableID id) {
  auto it = pollEntries.find(id);
  if (it != pollEntries.end()) {
    pollEntries.erase(it);
  }

  // Remove from pools
  poolManager.sockets.destroy(id);
  poolManager.listeners.destroy(id);
}

void Poller::enablePollout(PollableID socket_id) {
  pollout_pending[socket_id] = true;
}

void Poller::notify() {
  // Write a byte to the notification pipe to wake up poll()
  if (hasNotificationPipe()) {
    char byte = 1;
    if (write(notification_pipe[1], &byte, 1) == -1) {
       // Ignore error
    }
  }
}

void Poller::updatePollEvents() {
  for (auto &entry : pollout_pending) {
    auto socket_id = entry.first;
    auto &pending = entry.second;
    if (pending) {
      auto it = pollEntries.find(socket_id);
      if (it != pollEntries.end()) {
        it->second.events |= POLLOUT;
        pending = false;
      }
    }
  }
}

std::vector<PollerEvent> Poller::poll(int timeout_ms) {
  // Process any expired timers first
  processExpiredTimers();

  // Update poll events for any pending POLLOUT requests
  updatePollEvents();

  // Rebuild pollFds from pollEntries
  pollFds.clear();

  // Add notification pipe as first fd (if available)
  if (hasNotificationPipe()) {
    pollfd notification_pfd;
    notification_pfd.fd = notification_pipe[0];
    notification_pfd.events = POLLIN;
    notification_pfd.revents = 0;
    pollFds.push_back(notification_pfd);
  }

  for (const auto &pair : pollEntries) {
    const auto &entry = pair.second;
    pollfd pfd;
    pfd.fd = entry.pollable->file_descriptor;
    pfd.events = entry.events;
    pfd.revents = 0;
    pollFds.push_back(pfd);
  }

  int result = ::poll(pollFds.data(), pollFds.size(), timeout_ms);

  std::vector<PollerEvent> events;

  if (result < 0) {
    if (errno != EINTR) {
      LOG_ERROR("Poll error: ", strerror(errno));
    }
    return events;
  }

  if (result == 0) {
    return events;
  }

  // Check notification pipe first (index 0) if it exists
  bool has_notification_pipe = hasNotificationPipe();
  if (has_notification_pipe && pollFds.size() > 0 &&
      (pollFds[0].revents & POLLIN)) {
    drainNotificationPipe();
  }

  // Collect events
  size_t index = has_notification_pipe ? 1 : 0;
  for (const auto &pair : pollEntries) {
    const auto &id = pair.first;
    const auto &entry = pair.second;
    if (index >= pollFds.size())
      break;

    short revents = pollFds[index].revents;
    if (revents != 0) {
      events.push_back({id, revents});
      
      // If this was a POLLOUT event, we might want to auto-clear it or let the user handle it.
      // For now, we just report it. The user (SocketManager) should handle clearing POLLOUT if needed,
      // but typically POLLOUT is level-triggered so it will keep firing until we stop asking for it.
      // In the old code, we cleared it if write buffer was empty.
      // We will leave that logic to the SocketManager.
    }
    index++;
  }

  return events;
}

// Timer implementation
Poller::TimerID Poller::createTimer(uint64_t ms, bool is_interval) {
  TimerID id = next_timer_id++;
  auto now = SteadyClock::now();
  auto expiry = SteadyClock::addMilliseconds(now, ms);
  
  TimerEntry entry = {
      id,
      expiry,
      ms,
      is_interval,
      false // expired
  };
  
  timers[id] = entry;
  return id;
}

bool Poller::isTimerExpired(TimerID id) {
    auto it = timers.find(id);
    if (it != timers.end()) {
        return it->second.expired;
    }
    return false;
}

void Poller::resetTimer(TimerID id) {
    auto it = timers.find(id);
    if (it != timers.end()) {
        it->second.expired = false;
        if (it->second.repeat) {
            it->second.expiry = SteadyClock::addMilliseconds(SteadyClock::now(), it->second.interval);
        }
    }
}

void Poller::destroyTimer(TimerID id) {
  timers.erase(id);
}

Pollable* Poller::getPollable(PollableID id) {
    auto it = pollEntries.find(id);
    if (it != pollEntries.end()) {
        return it->second.pollable;
    }
    return nullptr;
}

void Poller::processExpiredTimers() {
  auto now = SteadyClock::now();

  for (auto &pair : timers) {
    TimerEntry &timer = pair.second;
    
    if (timer.expired) continue; // Already expired, waiting for reset

    if (SteadyClock::durationMs(timer.expiry, now) >= 0) {
      timer.expired = true;
    }
  }
}

// Pipe helper methods
bool Poller::createNotificationPipe() {
  if (notification_pipe[0] == -1 && notification_pipe[1] == -1) {
    if (pipe(notification_pipe) == -1) {
      notification_pipe[0] = -1;
      notification_pipe[1] = -1;
      return false;
    }

    // Make the read end non-blocking
    int flags = fcntl(notification_pipe[0], F_GETFL, 0);
    if (flags != -1) {
      fcntl(notification_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }

    return true;
  }
  return true; // Already created
}

void Poller::closeNotificationPipe() {
  if (notification_pipe[0] != -1) {
    close(notification_pipe[0]);
    notification_pipe[0] = -1;
  }
  if (notification_pipe[1] != -1) {
    close(notification_pipe[1]);
    notification_pipe[1] = -1;
  }
}

bool Poller::hasNotificationPipe() const { return notification_pipe[0] != -1; }

void Poller::drainNotificationPipe() {
  if (hasNotificationPipe()) {
    char buffer[256];
    while (read(notification_pipe[0], buffer, sizeof(buffer)) > 0) {
      // Just drain
    }
  }
}

} // namespace websrv