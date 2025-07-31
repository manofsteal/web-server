#include "poller.hpp"
#include <iostream>
#include <mutex>
#include <thread>
#include <algorithm>
#include <limits>

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

  while (running) {
    // Process any expired timers first
    processExpiredTimers();

    // Update poll events for any pending POLLOUT requests
    updatePollEvents();

    // Calculate timeout based on next timer expiry
    int timeout = calculatePollTimeout();

    // Rebuild pollFds from pollEntries
    pollFds.clear();
    for (const auto &pair : pollEntries) {
      const auto &id = pair.first;
      const auto &entry = pair.second;
      pollfd pfd;
      pfd.fd = entry.pollable->file_descriptor;
      pfd.events = entry.events;
      pfd.revents = 0;
      pollFds.push_back(pfd);
    }

    // If no pollable objects and no timers, sleep briefly
    if (pollFds.empty() && timers.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Use calculated timeout or 0 if we have no file descriptors but have timers
    int poll_timeout = pollFds.empty() ? 0 : timeout;
    int result = poll(pollFds.data(), pollFds.size(), poll_timeout);

    if (result < 0) {
      if (errno == EINTR)
        continue; // Interrupted by signal, continue
      std::cerr << "Poll error: " << strerror(errno) << std::endl;
      break;
    }

    if (result == 0) {
      // Timeout occurred - timers will be processed at the start of next loop
      continue;
    }

    // Process events
    size_t index = 0;
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
        case PollableType::TIMER:
          type_str = "TIMER";
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
}

// Timer implementation
Poller::TimerID Poller::setTimeout(uint32_t ms, TimerCallback callback) {
  TimerID id = next_timer_id++;
  auto now = std::chrono::steady_clock::now();
  auto expiry = now + std::chrono::milliseconds(ms);
  
  timers[id] = TimerEntry{
    id,
    expiry,
    ms,
    callback,
    false, // not an interval
    true   // active
  };
  
  return id;
}

Poller::TimerID Poller::setInterval(uint32_t ms, TimerCallback callback) {
  TimerID id = next_timer_id++;
  auto now = std::chrono::steady_clock::now();
  auto expiry = now + std::chrono::milliseconds(ms);
  
  timers[id] = TimerEntry{
    id,
    expiry,
    ms,
    callback,
    true, // is an interval
    true  // active
  };
  
  return id;
}

void Poller::clearTimeout(TimerID timer_id) {
  auto it = timers.find(timer_id);
  if (it != timers.end()) {
    timers.erase(it);
  }
}

void Poller::clearInterval(TimerID timer_id) {
  auto it = timers.find(timer_id);
  if (it != timers.end()) {
    timers.erase(it);
  }
}

int Poller::calculatePollTimeout() {
  if (timers.empty()) {
    return 1000; // Default 1 second timeout
  }
  
  auto now = std::chrono::steady_clock::now();
  auto next_expiry = std::chrono::steady_clock::time_point::max();
  
  // Find the earliest timer expiry
  for (const auto &pair : timers) {
    const auto &id = pair.first;
    const auto &timer = pair.second;
    if (timer.active && timer.expiry_time < next_expiry) {
      next_expiry = timer.expiry_time;
    }
  }
  
  if (next_expiry == std::chrono::steady_clock::time_point::max()) {
    return 1000; // No active timers, use default timeout
  }
  
  // Calculate milliseconds until next expiry
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(next_expiry - now);
  int timeout_ms = static_cast<int>(duration.count());
  
  // Ensure we don't return negative timeout or wait too long
  return std::max(0, std::min(timeout_ms, 1000));
}

void Poller::processExpiredTimers() {
  auto now = std::chrono::steady_clock::now();
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
        timer.expiry_time = now + std::chrono::milliseconds(timer.interval_ms);
      } else {
        // Remove one-time timer
        timers.erase(it);
      }
    }
  }
}