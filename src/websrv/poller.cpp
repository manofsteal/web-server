#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

namespace websrv {

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
  // If called from the same thread as the poller, do nothing
  if (std::this_thread::get_id() == poller_thread_id) {
    return;
  }

  // Write a byte to the notification pipe to wake up poll()
  if (hasNotificationPipe()) {
    char byte = 1;

    if (write(notification_pipe[1], &byte, 1) == -1) {
      // Handle error but don't throw - this might be called from signal
      // handlers
      LOG_ERROR("Failed to write to notification pipe: ", strerror(errno));
    }
  }
}

void Poller::setMaxPollTimeout(int max_timeout_ms) {
  max_poll_timeout_ms = std::max(2000, max_timeout_ms);
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

void Poller::start() {
  running = true;

  // Capture the poller thread ID
  poller_thread_id = std::this_thread::get_id();

  // Create notification pipe at start
  createNotificationPipe();

  while (running) {
    // Process any expired timers first
    processExpiredTimers();

    // Update poll events for any pending POLLOUT requests
    updatePollEvents();

    // Calculate timeout based on next timer expiry
    current_poll_timeout_ms = calculatePollTimeout();

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
      const auto &id = pair.first;
      const auto &entry = pair.second;
      pollfd pfd;
      pfd.fd = entry.pollable->file_descriptor;
      pfd.events = entry.events;
      pfd.revents = 0;
      pollFds.push_back(pfd);
    }

    int result = poll(pollFds.data(), pollFds.size(), current_poll_timeout_ms);

    if (result < 0) {
      if (errno == EINTR)
        continue; // Interrupted by signal, continue
      LOG_ERROR("Poll error: ", strerror(errno));
      break;
    }

    if (result == 0) {
      // Timeout occurred - timers will be processed at the start of next loop
      continue;
    }

    // Process events
    size_t index = 0;

    // Check notification pipe first (index 0) if it exists
    bool has_notification_pipe = hasNotificationPipe();
    if (has_notification_pipe && pollFds.size() > 0 &&
        pollFds[0].revents & POLLIN) {
      drainNotificationPipe();
    }

    processCleanupTasks();

    // Process pollable events (starting after notification pipe if it exists)
    index = has_notification_pipe ? 1 : 0;
    for (const auto &pair : pollEntries) {
      const auto &id = pair.first;
      const auto &entry = pair.second;
      if (index >= pollFds.size())
        break;

      short revents = pollFds[index].revents;
      if (revents != 0) {

        entry.pollable->onEvent(revents);
        std::string type_str;
        switch (entry.pollable->type) {
        case PollableType::SOCKET:
          type_str = "SOCKET";
          break;
        case PollableType::LISTENER:
          type_str = "LISTENER";
          break;
        default:
          type_str = "UNKNOWN";
          break;
        }
        // std::cout << "Event: " << revents << " for " << type_str <<
        // std::endl;

        // If this was a POLLOUT event and write buffer is now empty, disable
        // POLLOUT
        if (revents & POLLOUT && entry.pollable->type == PollableType::SOCKET) {
          Socket *socket = static_cast<Socket *>(entry.pollable);
          if (socket->write_buffer.size() == 0) {
            // Remove POLLOUT from events to prevent busy loop
            const_cast<PollEntry &>(entry).events &= ~POLLOUT;
          }
        }
      }
      index++;
    }
  }
}

void Poller::stop() {
  running = false;
  // Stop executor first
  executor.stop();

  for (auto &pair : pollEntries) {
    auto &id = pair.first;
    auto &entry = pair.second;
    entry.pollable->stopFunction();
  }

  // Close notification pipe
  closeNotificationPipe();
}

// Timer implementation
Poller::TimerID Poller::setTimeout(uint32_t ms, TimerCallback callback) {
  TimerID id = next_timer_id++;
  auto now = SteadyClock::now();
  auto expiry = SteadyClock::addMilliseconds(now, ms);

  timers[id] = TimerEntry{
      id,    expiry, ms, callback,
      false, // not an interval
      true   // active
  };

  // If poller is running, notify to recalculate timeout
  // (especially important when called from within timer callbacks)
  if (running) {
    notify();
  }

  return id;
}

Poller::TimerID Poller::setInterval(uint32_t ms, TimerCallback callback) {
  TimerID id = next_timer_id++;
  auto now = SteadyClock::now();
  auto expiry = SteadyClock::addMilliseconds(now, ms);

  timers[id] = TimerEntry{
      id,   expiry, ms, callback,
      true, // is an interval
      true  // active
  };

  // If new timer has smaller timeout than current poll timeout, notify to
  // recalculate
  if (running && (int)ms < current_poll_timeout_ms) {
    notify();
  }

  return id;
}

void Poller::clearTimeout(TimerID timer_id) {
  cleanupTasks.push_back([this, timer_id]() {
    auto it = timers.find(timer_id);
    if (it != timers.end()) {
      timers.erase(it);
    }
  });
}

void Poller::clearInterval(TimerID timer_id) {
  cleanupTasks.push_back([this, timer_id]() {
    auto it = timers.find(timer_id);
    if (it != timers.end()) {
      timers.erase(it);
    }
  });
}

void Poller::processCleanupTasks() {
  for (auto &task : cleanupTasks) {
    task();
  }
  cleanupTasks.clear();
}

int Poller::calculatePollTimeout() {
  if (timers.empty()) {
    return max_poll_timeout_ms;
  }

  auto now = SteadyClock::now();
  auto next_expiry = SteadyClock::TimePoint::max();

  // Find the earliest timer expiry
  for (const auto &pair : timers) {
    const auto &id = pair.first;
    const auto &timer = pair.second;
    if (timer.active && timer.expiry_time < next_expiry) {
      next_expiry = timer.expiry_time;
    }
  }

  if (next_expiry == SteadyClock::TimePoint::max()) {
    return max_poll_timeout_ms; // No active timers, use default timeout
  }

  // Calculate milliseconds until next expiry
  int timeout_ms = SteadyClock::durationMs(now, next_expiry);

  // Ensure we don't return negative timeout or wait too long
  return std::max(1, std::min(timeout_ms, max_poll_timeout_ms));
}

void Poller::processExpiredTimers() {
  auto now = SteadyClock::now();
  std::vector<TimerID> expired_timers;

  // Find expired timers
  for (auto &pair : timers) {
    const auto &id = pair.first;
    auto &timer = pair.second;
    if (timer.active && timer.expiry_time <= now) {
      expired_timers.push_back(id);
    }
  }

  // Process expired timers
  for (TimerID id : expired_timers) {
    auto it = timers.find(id);
    if (it != timers.end()) {
      TimerEntry &timer = it->second;

      // Execute callback
      timer.callback();

      if (timer.is_interval) {
        // Reschedule interval timer
        timer.expiry_time =
            SteadyClock::addMilliseconds(now, timer.interval_ms);
      } else {
        // Remove one-time timer
        timers.erase(it);
      }
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
    ssize_t bytes_read;

    while ((bytes_read = read(notification_pipe[0], buffer, sizeof(buffer))) >
           0) {
      // Just drain the pipe, no action needed
    }

    // Check if we stopped because of EAGAIN/EWOULDBLOCK (normal for
    // non-blocking) or because of an actual error
    if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_ERROR("Error draining notification pipe: ", strerror(errno));
    }
  }
}

} // namespace websrv