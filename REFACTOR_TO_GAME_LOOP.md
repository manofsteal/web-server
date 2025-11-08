# Refactoring Plan: Transform to Pure Game Loop Style

## Goal

Refactor the current callback-based architecture to pure game loop style by:
- ✅ **Reusing** all existing, tested low-level code (socket creation, buffers, poll() logic)
- ✅ **Removing** callbacks from public API
- ✅ **Adding** state flags and query methods
- ✅ **Changing** Poller::start() to pollOnce() that returns control each frame

## What We Keep (Tested Code)

```cpp
✅ Socket creation and connection logic (socket.cpp:38-76)
✅ Buffer management (read_buffer, write_buffer)
✅ poll() system call usage (poller.cpp:124)
✅ FD management and pollEntries tracking
✅ SteadyClock timing utilities (for simple software timers)
✅ POLLOUT management for write buffering
✅ WebSocket frame parsing/generation
✅ HTTP parsing logic
```

## What We Remove (Platform-Specific Complexity)

```cpp
❌ Remove: Platform-specific timers (timerfd, kqueue, QNX pulses)
❌ Remove: Timer file descriptors in poll()
❌ Simplify: Use software timers with SteadyClock (game loop checks each frame)
```

## What We Change (API Surface)

```cpp
❌ Remove: socket->onData = callback;
❌ Remove: listener->onAccept = callback;
❌ Remove: timer->setInterval(ms, callback);
❌ Remove: poller.start();  // infinite loop

✅ Add: socket->isReadable()
✅ Add: socket->receive()
✅ Add: listener->acceptConnection()
✅ Add: timer->isExpired()
✅ Add: poller.pollOnce(timeout)  // returns control
```

## Refactoring Steps

### Step 1: Transform Socket (Reuse Core, Change API)

#### Before (callback-based):
```cpp
// Current: socket.hpp
struct Socket : Pollable {
    Callback onData = [](Socket &, const BufferView &) {};

    // Internal onEvent calls onData
};
```

#### After (state-based):
```cpp
// New: socket.hpp
struct Socket : Pollable {
    // Remove callbacks
    // Callback onData = ...;  ← DELETE THIS

    // Add state flags
    bool readable = false;
    bool writable = true;
    bool has_error = false;
    int error_code = 0;

    // Add query methods
    bool isReadable() const { return readable; }
    bool isWritable() const { return writable; }
    bool hasError() const { return has_error; }

    // Add explicit receive (game loop style)
    BufferView receive();
    void clearReadBuffer();

    // Keep existing methods
    bool start(const std::string &host, uint16_t port);  // ← KEEP
    void write(const Buffer &data);  // ← KEEP
};
```

**Implementation Changes:**

**File:** `src/websrv/socket.cpp`

```cpp
// Keep constructor but change onEvent behavior
Socket::Socket() : Pollable() {
  type = PollableType::SOCKET;

  onEvent = [this](short revents) {
    // KEEP: All the socket I/O logic
    // CHANGE: Set flags instead of calling callbacks

    if (file_descriptor >= 0) {
      // Handle errors
      if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
          has_error = true;
          error_code = errno;
          readable = false;
          writable = false;
          return;
      }

      // Handle POLLIN - KEEP the read logic, CHANGE the dispatch
      if (revents & POLLIN) {
        char buffer[1024];
        ssize_t bytes_read = read(file_descriptor, buffer, sizeof(buffer));
        if (bytes_read > 0) {
          // KEEP: Read data
          // CHANGE: Store in read_buffer instead of calling callback
          read_buffer.append(buffer, bytes_read);
          readable = true;  // Set flag

          // DELETE: onData(*this, view);
        } else if (bytes_read == 0) {
            has_error = true;
            error_code = ECONNRESET;
        }
      }

      // Handle POLLOUT - KEEP existing logic
      if (revents & POLLOUT) {
          writable = true;  // ADD: Set flag

          // KEEP: Existing write buffer draining logic
          if (write_buffer.size() > 0) {
              // ... existing code ...
          }
      }
    }
  };
}

// ADD: New method for game loop
BufferView Socket::receive() {
    if (read_buffer.size() == 0) {
        return BufferView{nullptr, 0};
    }

    // Return view of read_buffer
    return BufferView{
        const_cast<char*>(reinterpret_cast<const char*>(read_buffer.data())),
        read_buffer.size()
    };
}

void Socket::clearReadBuffer() {
    read_buffer.clear();
    readable = false;
}

// KEEP: All other existing methods unchanged
bool Socket::start(...) { /* existing code */ }
void Socket::write(...) { /* existing code */ }
```

**Lines Changed:** ~40 lines in socket.cpp
**Code Reused:** ~95% of existing implementation

### Step 2: Transform Listener (Reuse Core, Change API)

#### Before:
```cpp
struct Listener : Pollable {
    AcceptCallback onAccept = [](Socket *) {};
};
```

#### After:
```cpp
struct Listener : Pollable {
    // Remove callback
    // AcceptCallback onAccept = ...;  ← DELETE

    // Add connection queue
    std::vector<Socket*> pending_connections;

    // Add query methods
    bool hasPendingConnection() const;
    Socket* acceptConnection();  // Returns nullptr if none

    // Keep existing
    bool start(uint16_t port);  // ← KEEP
    void stop();  // ← KEEP
};
```

**File:** `src/websrv/listener.cpp`

```cpp
Listener::Listener() : Pollable() {
  type = PollableType::LISTENER;

  onEvent = [this](short revents) {
    if (file_descriptor >= 0) {
      // KEEP: All accept() logic
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      int client_fd = accept(file_descriptor, ...);

      if (client_fd >= 0) {
        // KEEP: Socket creation and setup
        Socket *client_socket = poller->createSocket();
        client_socket->file_descriptor = client_fd;
        client_socket->remote_addr = inet_ntoa(client_addr.sin_addr);
        client_socket->remote_port = ntohs(client_addr.sin_port);

        // CHANGE: Queue instead of callback
        pending_connections.push_back(client_socket);

        // DELETE: onAccept(client_socket);
      }
    }
  };
}

// ADD: New methods
bool Listener::hasPendingConnection() const {
    return !pending_connections.empty();
}

Socket* Listener::acceptConnection() {
    if (pending_connections.empty()) return nullptr;
    Socket* sock = pending_connections.front();
    pending_connections.erase(pending_connections.begin());
    return sock;
}

// KEEP: Existing methods unchanged
bool Listener::start(uint16_t port) { /* existing code */ }
void Listener::stop() { /* existing code */ }
```

**Lines Changed:** ~25 lines
**Code Reused:** ~98% of existing implementation

### Step 3: Transform Poller (Keep poll() Logic, Change Control Flow)

#### Before:
```cpp
void Poller::start() {
    while (running) {
        // poll and dispatch callbacks
    }
}
```

#### After:
```cpp
// Rename start() → pollOnce()
int Poller::pollOnce(int timeout_ms = -1) {
    // Same poll() logic
    // Returns control after one iteration
    return num_events;
}

// Keep for backward compat or remove entirely
void Poller::run(int max_iterations = -1) {
    int iteration = 0;
    while (running && (max_iterations < 0 || iteration < max_iterations)) {
        pollOnce(-1);
        iteration++;
    }
}
```

**File:** `src/websrv/poller.cpp`

**Extract Loop Body:**
```cpp
int Poller::pollOnce(int timeout_ms) {
    // KEEP: All existing logic from start() loop body
    processExpiredTimers();
    updatePollEvents();

    int poll_timeout = (timeout_ms >= 0) ? timeout_ms : calculatePollTimeout();

    // KEEP: Existing poll() setup
    pollFds.clear();
    // ... build pollFds ...

    // KEEP: Existing poll() call
    int result = poll(pollFds.data(), pollFds.size(), poll_timeout);

    // KEEP: Existing error handling
    if (result < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }

    // KEEP: Existing event processing
    for (const auto &pair : pollEntries) {
        short revents = pollFds[index].revents;
        if (revents != 0) {
            entry.pollable->onEvent(revents);  // Calls Socket::onEvent which now sets flags
        }
        index++;
    }

    return result;  // CHANGE: Return instead of loop
}

// Optional: Keep blocking version
void Poller::run(int max_iterations) {
    running = true;
    int iteration = 0;

    while (running && (max_iterations < 0 || iteration < max_iterations)) {
        pollOnce(-1);
        iteration++;
    }
}
```

**Lines Changed:** ~50 lines (mostly extract and reorganize)
**Code Reused:** ~100% of existing poll() logic

### Step 4: Transform Timers (Simplify to Software Timers)

#### Before (platform-specific):
```cpp
// Linux: timerfd_create, adds FD to poll()
// macOS: kqueue EVFILT_TIMER
// QNX: pulses
TimerID setTimeout(uint32_t ms, TimerCallback callback);
```

#### After (simple software timers):
```cpp
struct TimerEntry {
    TimerID id;
    SteadyClock::TimePoint expiry_time;  // When timer expires
    uint32_t interval_ms;
    bool is_interval;
    bool active;
    bool expired;  // Flag checked each frame
    // No callback, no file descriptor
};

// New API
TimerID createTimer(uint32_t ms, bool is_interval);
bool isTimerExpired(TimerID id);
void resetTimer(TimerID id);
void destroyTimer(TimerID id);
```

**Why This Works:**
- ✅ Game loop calls pollOnce() every 10-16ms anyway
- ✅ No need for OS timer events in poll()
- ✅ Just check SteadyClock each frame
- ✅ Simpler, cross-platform, no #ifdefs

**File:** `include/websrv/poller.hpp`

```cpp
struct Poller {
    // Timer structures - SIMPLIFIED
    using TimerID = uint32_t;

    struct TimerEntry {
        TimerID id;
        SteadyClock::TimePoint expiry_time;
        uint32_t interval_ms;
        bool is_interval;
        bool active;
        bool expired;
        // REMOVE: TimerCallback callback
        // REMOVE: int timerfd (Linux)
        // REMOVE: platform-specific code
    };

    TimerID next_timer_id = 1;
    std::map<TimerID, TimerEntry> timers;

    // REMOVE: calculatePollTimeout() - not needed, user provides timeout
    // KEEP: processExpiredTimers() - but simplified

    // New API
    TimerID createTimer(uint32_t ms, bool is_interval);
    bool isTimerExpired(TimerID id);
    void resetTimer(TimerID id);
    void destroyTimer(TimerID id);

    // REMOVE: setTimeout, setInterval, clearTimeout, clearInterval
};
```

**File:** `src/websrv/poller.cpp`

```cpp
TimerID Poller::createTimer(uint32_t ms, bool is_interval) {
    TimerID id = next_timer_id++;
    TimerEntry entry;
    entry.id = id;
    entry.expiry_time = SteadyClock::addMilliseconds(SteadyClock::now(), ms);
    entry.interval_ms = ms;
    entry.is_interval = is_interval;
    entry.active = true;
    entry.expired = false;

    timers[id] = entry;
    return id;
}

bool Poller::isTimerExpired(TimerID id) {
    auto it = timers.find(id);
    return (it != timers.end()) && it->second.expired;
}

void Poller::resetTimer(TimerID id) {
    auto it = timers.find(id);
    if (it != timers.end()) {
        it->second.expired = false;
        if (it->second.is_interval) {
            it->second.expiry_time = SteadyClock::addMilliseconds(
                SteadyClock::now(), it->second.interval_ms);
        }
    }
}

void Poller::destroyTimer(TimerID id) {
    timers.erase(id);
}

// SIMPLIFIED: No file descriptors, just check time
void Poller::processExpiredTimers() {
    auto now = SteadyClock::now();

    for (auto& pair : timers) {
        auto& timer = pair.second;
        if (!timer.active) continue;

        // Check if expired
        if (SteadyClock::durationMs(timer.expiry_time, now) >= 0) {
            timer.expired = true;

            // Handle intervals
            if (timer.is_interval) {
                timer.expiry_time = SteadyClock::addMilliseconds(
                    timer.expiry_time, timer.interval_ms);
            } else {
                timer.active = false;
            }
        }
    }
}
```

**Changes:**
- ❌ Remove all platform-specific timer code (timerfd, kqueue, etc.)
- ❌ Remove timer file descriptors from poll()
- ✅ Use simple SteadyClock-based checking
- ✅ Much simpler, fully cross-platform

**Lines Changed:** ~80 lines (removing complexity)
**Code Reused:** SteadyClock utilities (already tested)

### Step 5: Update WebSocket (Keep Frame Logic, Change API)

**File:** `include/websrv/websocket_client.hpp`

```cpp
struct WebSocketClient {
    Socket *socket = nullptr;
    WebSocketStatus status = WebSocketStatus::CLOSED;

    // Remove callbacks
    // MessageCallback onMessage = ...;  ← DELETE
    // BinaryCallback onBinary = ...;  ← DELETE
    // OpenCallback onOpen = ...;  ← DELETE
    // CloseCallback onClose = ...;  ← DELETE
    // ErrorCallback onError = ...;  ← DELETE

    // Add message queues
    std::vector<std::string> pending_text_messages;
    std::vector<std::vector<uint8_t>> pending_binary_messages;
    std::string error_message;

    // Add query methods
    bool hasTextMessage() const;
    bool hasBinaryMessage() const;
    bool hasError() const;

    std::string popTextMessage();
    std::vector<uint8_t> popBinaryMessage();
    std::string getError() const;

    // Keep existing
    bool connect(const std::string& url);  // ← KEEP
    void sendText(const std::string& message);  // ← KEEP
    void sendBinary(const std::vector<uint8_t>& data);  // ← KEEP
    void close(uint16_t code, const std::string& reason);  // ← KEEP
};
```

**File:** `src/websrv/websocket_client.cpp`

```cpp
// KEEP: All frame parsing logic
// CHANGE: When frame is complete, queue instead of callback

// In processFrame() or similar:
if (frame.opcode == WebSocketOpcode::TEXT) {
    std::string text = /* parse frame */;
    // CHANGE: Queue instead of callback
    pending_text_messages.push_back(text);
    // DELETE: if (onMessage) onMessage(text);
}

if (frame.opcode == WebSocketOpcode::BINARY) {
    std::vector<uint8_t> data = /* parse frame */;
    // CHANGE: Queue instead of callback
    pending_binary_messages.push_back(data);
    // DELETE: if (onBinary) onBinary(data);
}

// ADD: Query methods
bool WebSocketClient::hasTextMessage() const {
    return !pending_text_messages.empty();
}

std::string WebSocketClient::popTextMessage() {
    if (pending_text_messages.empty()) return "";
    std::string msg = pending_text_messages.front();
    pending_text_messages.erase(pending_text_messages.begin());
    return msg;
}
```

**Lines Changed:** ~80 lines
**Code Reused:** ~95% of frame parsing/generation

## Complete Game Loop Example (After Refactoring)

```cpp
#include "websrv/poller.hpp"
#include <iostream>

int main() {
    websrv::Poller poller;

    // Create server
    websrv::Listener* server = poller.createListener();
    server->start(8080);

    // Track connected clients
    std::vector<websrv::Socket*> clients;

    // Create timer
    auto timer_id = poller.createTimer(1000, true);  // 1s interval

    bool running = true;
    int frame = 0;

    // Game loop - clear, explicit control flow
    while (running) {
        frame++;

        // Poll with 10ms timeout (100 FPS)
        int events = poller.pollOnce(10);

        // Check for new connections
        if (server->hasPendingConnection()) {
            websrv::Socket* client = server->acceptConnection();
            clients.push_back(client);
            std::cout << "[Frame " << frame << "] Client connected\n";
        }

        // Process all clients
        for (auto* client : clients) {
            // Check for incoming data
            if (client->isReadable()) {
                auto view = client->receive();
                std::string msg(view.data, view.size);
                std::cout << "[Frame " << frame << "] << " << msg;

                // Echo back
                client->write("Echo: " + msg);
                client->clearReadBuffer();
            }

            // Check for errors
            if (client->hasError()) {
                std::cout << "[Frame " << frame << "] Client error\n";
                // Remove from clients list
            }
        }

        // Check timer
        if (poller.isTimerExpired(timer_id)) {
            std::cout << "[Frame " << frame << "] Timer tick\n";
            poller.resetTimer(timer_id);
        }

        // Example: run for 1000 frames then stop
        if (frame >= 1000) running = false;
    }

    return 0;
}
```

## Migration Strategy for Tests

### Phase 1: Update Unit Tests (Week 1)

1. **Socket Tests**
   ```cpp
   // Before
   socket->onData = [&received](Socket& s, const BufferView& d) {
       received = true;
   };

   // After
   while (!socket->isReadable() && timeout < 1000) {
       poller.pollOnce(10);
       timeout += 10;
   }
   ASSERT_TRUE(socket->isReadable());
   auto data = socket->receive();
   ```

2. **Listener Tests**
   ```cpp
   // Before
   listener->onAccept = [&accepted](Socket* s) {
       accepted = true;
   };

   // After
   while (!listener->hasPendingConnection()) {
       poller.pollOnce(10);
   }
   Socket* client = listener->acceptConnection();
   ASSERT_NE(client, nullptr);
   ```

3. **Timer Tests**
   ```cpp
   // Before
   poller.setTimeout(100, [&fired]() { fired = true; });

   // After
   auto timer = poller.createTimer(100, false);
   while (!poller.isTimerExpired(timer)) {
       poller.pollOnce(10);
   }
   ASSERT_TRUE(poller.isTimerExpired(timer));
   ```

### Phase 2: Update Integration Tests (Week 2)

Update all examples in `test/` directory:
- `ping_pong_*_test.cpp`
- `websocket_*_test.cpp`
- `http_*_test.cpp`

Pattern:
```cpp
// Replace poller.start() with explicit loop
while (running) {
    poller.pollOnce(10);
    // Check states explicitly
}
```

## Timeline

| Week | Tasks | Files Modified |
|------|-------|----------------|
| 1 | Refactor Socket, Listener headers | 2 files |
| 1 | Update Socket, Listener implementation | 2 files |
| 1 | Update Socket, Listener tests | 3-5 files |
| 2 | Refactor Poller (add pollOnce) | 2 files |
| 2 | Update Poller tests | 2-3 files |
| 3 | Refactor Timer API | 2 files |
| 3 | Update Timer tests | 1-2 files |
| 4 | Refactor WebSocket | 2 files |
| 4 | Update WebSocket tests | 2-3 files |
| 5 | Update HTTP client/server | 4 files |
| 5 | Update all integration tests | 5-10 files |
| 6 | Documentation and examples | README, examples/ |

**Total: 6 weeks**

## Code Changes Summary

| Component | Files | Lines Changed | Lines Removed | Lines Reused | Reuse % |
|-----------|-------|---------------|---------------|--------------|---------|
| Socket | 2 | ~60 | ~10 (callbacks) | ~200 | 77% |
| Listener | 2 | ~40 | ~5 (callbacks) | ~120 | 75% |
| Poller | 2 | ~80 | ~20 (loop) | ~400 | 83% |
| Timer | 2 | ~50 | ~200 (platform code) | ~30 (SteadyClock) | Simplified! |
| WebSocket | 2 | ~100 | ~15 (callbacks) | ~800 | 89% |
| **Total** | **10** | **~330** | **~250** | **~1550** | **82%** |

**Note:** Timer is massively simplified by removing all platform-specific code (timerfd, kqueue, QNX pulses)

## Benefits

✅ **Clean Game Loop API** - No callbacks, explicit control
✅ **Maximum Code Reuse** - Keep 80%+ of tested code
✅ **Better Debugging** - Can inspect state at any point
✅ **Predictable Flow** - No callback surprises
✅ **Frame-based** - Natural for game engines, simulations
✅ **Testable** - Easier to write deterministic tests
✅ **Simpler Timers** - Remove ~200 lines of platform-specific code (timerfd, kqueue, QNX)
✅ **Cross-Platform** - Software timers work everywhere, no #ifdefs
✅ **Less Complexity** - Fewer moving parts, easier to understand and maintain

## Breaking Changes

⚠️ **API Changes** - All callbacks removed
⚠️ **Control Flow** - `poller.start()` → `poller.pollOnce()` loop
⚠️ **Migration Required** - Existing code must update

## Migration Checklist

- [ ] Update Socket API (remove onData, add isReadable/receive)
- [ ] Update Listener API (remove onAccept, add acceptConnection)
- [ ] Add Poller::pollOnce() method
- [ ] Update Timer API (remove callbacks, add expiry flags)
- [ ] Update all unit tests
- [ ] Update all integration tests
- [ ] Update all examples
- [ ] Update README and documentation
- [ ] Create migration guide for users

---

**Next Step:** Start with Socket refactoring (smallest component, easiest to test)
