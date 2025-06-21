#include "poller.hpp"
#include <iostream>
#include <mutex>
#include <thread>

void Poller::cleanup() {
  // Stop executor first
  executor.stop();

  for (auto &[id, entry] : pollEntries) {
    // entry.cleanup();
  }
  pollEntries.clear();
  poolManager.cleanup();
}

// Factory methods
Socket *Poller::createSocket() {
  Socket *socket = poolManager.sockets.create(&poolManager.id_manager);
  addSocket(socket);
  return socket;
}

Timer *Poller::createTimer() {
  Timer *timer = poolManager.timers.create(&poolManager.id_manager);
  addTimer(timer);
  return timer;
}

Listener *Poller::createListener() {
  Listener *listener = poolManager.listeners.create(&poolManager.id_manager);
  addListener(listener);
  return listener;
}

void Poller::addSocket(Socket *socket) {
  if (!socket)
    return;
  socket->poller = this;
  pollEntries[socket->id] = PollEntry{socket, POLLIN};
}

void Poller::addTimer(Timer *timer) {
  if (!timer)
    return;

  timer->poller = this;
  pollEntries[timer->id] = PollEntry{timer, POLLIN};
}

void Poller::addListener(Listener *listener) {
  if (!listener)
    return;

  listener->poller = this;
  pollEntries[listener->id] = PollEntry{listener, POLLIN};
}

void Poller::remove(PollableID id) {
  auto it = pollEntries.find(id);
  if (it != pollEntries.end()) {
    pollEntries.erase(it);
  }

  // Remove from pools
  poolManager.timers.destroy(id);
  poolManager.sockets.destroy(id);
  poolManager.listeners.destroy(id);
}

void Poller::enableSocketPollout(PollableID socket_id) {
  pollout_pending[socket_id] = true;
}

void Poller::updatePollEvents() {
  for (auto &[socket_id, pending] : pollout_pending) {
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
    // Update poll events for any pending POLLOUT requests
    updatePollEvents();

    // Rebuild pollFds from pollEntries
    pollFds.clear();
    for (const auto &[id, entry] : pollEntries) {
      pollfd pfd;
      pfd.fd = entry.pollable->file_descriptor;
      pfd.events = entry.events;
      pfd.revents = 0;
      pollFds.push_back(pfd);
    }

    if (pollFds.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    int result = poll(pollFds.data(), pollFds.size(), 1000); // 1 second timeout

    if (result < 0) {
      if (errno == EINTR)
        continue; // Interrupted by signal, continue
      std::cerr << "Poll error: " << strerror(errno) << std::endl;
      break;
    }

    if (result == 0)
      continue; // Timeout, no events

    // Process events
    size_t index = 0;
    for (const auto &[id, entry] : pollEntries) {
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
  // Executor will be stopped in cleanup()
}