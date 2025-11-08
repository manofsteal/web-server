# Plan: Poll-Based Event Loop Library (Game Loop Style)

## Overview
Design and implement a synchronous, poll-based event loop library that eliminates callbacks in favor of explicit per-frame processing, similar to game engine architecture.

## Core Architecture

### 1. Event Loop Core (`event_loop.h/cpp`)

**Responsibilities:**
- Manage file descriptors using `poll()` syscall
- Track all registered FDs with their types (socket, timer, signal, etc.)
- Return raw poll events each frame
- Handle timeout management


### 4. Signal Manager (`signal_manager.h/cpp`)

**Responsibilities:**
- Handle POSIX signals using signalfd
- Process signal events

**Key Components:**
```cpp
SignalManager:
  - int signalfd
  - set<int> monitored_signals
  - vector<SignalEvent> pending_signals
  
  SignalEvent:
  - int signo
  - pid_t sender_pid
  
  Methods:
  - add_signal(signo)
  - process(events)  // reads signalfd, populates pending_signals
  - poll_signals() -> vector<SignalEvent>
  - clear_signals()
```

## Implementation Phases

### Phase 1: Event Loop Core (Week 1)
- Implement EventLoop with poll() wrapper
- FD registration/unregistration
- Event type tagging system
- Basic poll event structure
- Unit tests for registration/polling

**Deliverables:**
- `event_loop.h` - EventLoop class interface
- `event_loop.cpp` - Implementation with poll() syscall
- `test_event_loop.cpp` - Unit tests
- Basic documentation

### Phase 2: Timer Manager (Week 1-2)
- timerfd creation and management
- One-shot and periodic timer support
- Event processing and flag setting
- Integration with EventLoop
- Timer accuracy tests

**Deliverables:**
- `timer_manager.h` - TimerManager class interface
- `timer_manager.cpp` - Implementation with timerfd
- `test_timer_manager.cpp` - Unit tests
- Timer accuracy benchmarks

### Phase 3: Socket Manager - Basic (Week 2-3)
- TCP client socket creation
- Non-blocking connect handling
- Basic send/receive with buffers
- Socket state machine
- Process events and flag updates
- TCP client connection tests

**Deliverables:**
- `socket_manager.h` - SocketManager class interface (basic)
- `socket_manager.cpp` - TCP client implementation
- `test_socket_manager_basic.cpp` - TCP client tests
- Connection state machine documentation

### Phase 4: Socket Manager - Advanced (Week 3-4)
- TCP server socket (listen/accept)
- UDP socket support
- Connection state tracking
- Error handling (POLLHUP, POLLERR)
- Backpressure handling (write buffer full)
- Multi-socket tests

**Deliverables:**
- Enhanced `socket_manager.h/cpp` - Full TCP/UDP support
- `test_socket_manager_advanced.cpp` - Server and UDP tests
- Error handling documentation
- Performance benchmarks

### Phase 5: Signal Manager (Week 4)
- signalfd integration
- Signal mask management
- Signal event processing
- SIGINT/SIGTERM handling tests

**Deliverables:**
- `signal_manager.h` - SignalManager class interface
- `signal_manager.cpp` - Implementation with signalfd
- `test_signal_manager.cpp` - Unit tests
- Signal handling guide

### Phase 6: Integration & Examples (Week 5)
- Complete game-loop style example
- HTTP client example (connect, send GET, receive)
- Echo server example
- Multi-timer example
- Performance benchmarks

**Deliverables:**
- `examples/game_loop.cpp` - Basic game loop pattern
- `examples/http_client.cpp` - HTTP GET request example
- `examples/echo_server.cpp` - TCP echo server
- `examples/multi_timer.cpp` - Multiple timer example
- Performance comparison report
- Complete API documentation

## Key Design Decisions

### Non-blocking I/O
All sockets set to `O_NONBLOCK` to prevent blocking during read/write operations within managers.

**Rationale:** Ensures poll() returns immediately when no data is available, maintaining consistent frame timing.

### State Management
Each manager maintains explicit state flags updated during `process()`. User code checks flags instead of receiving callbacks.

**Rationale:** 
- Predictable control flow
- Easy debugging (inspect state at any point)
- No callback registration/deregistration complexity
- Natural integration with game loop patterns

### Buffer Management
- SocketManager owns read/write buffers per socket
- User calls `send()` to queue data (adds to write_buffer)
- Manager writes when socket is writable
- User calls `receive()` to get data from read_buffer

**Rationale:**
- Handles partial writes transparently
- Prevents busy-waiting on write operations
- Allows batching of small writes
- User can check buffer status before sending more data

### Error Handling
Errors reported via flags (has_error) and retrievable error codes. No exceptions thrown from managers.

**Rationale:**
- Exceptions break game loop flow
- Explicit error checking is clearer
- Better for real-time systems
- User decides when to handle errors

### Thread Safety
Single-threaded design. All operations happen on one thread per event loop instance.

**Rationale:**
- Simpler implementation
- No locking overhead
- Matches game loop pattern
- Multiple event loops can run in different threads if needed

## Example Usage Pattern

```cpp
#include "event_loop.h"
#include "timer_manager.h"
#include "socket_manager.h"
#include "signal_manager.h"

int main() {
    EventLoop event_loop;
    TimerManager timer_mgr(event_loop);
    SocketManager socket_mgr(event_loop);
    SignalManager signal_mgr(event_loop);

    // Setup
    signal_mgr.add_signal(SIGINT);
    signal_mgr.add_signal(SIGTERM);
    
    auto timer_id = timer_mgr.create_timer(1000, true);  // 1s periodic
    auto sock_id = socket_mgr.create_tcp_client("example.com", 80);
    
    bool running = true;

    // Game loop
    while (running) {
        // Poll with 10ms timeout
        auto events = event_loop.poll(10);
        
        // Process all managers
        timer_mgr.process(events);
        socket_mgr.process(events);
        signal_mgr.process(events);
        
        // Check timer
        if (timer_mgr.is_expired(timer_id)) {
            std::cout << "Timer tick at " << get_time() << "\n";
            timer_mgr.reset_expired(timer_id);
        }
        
        // Check socket connection
        if (socket_mgr.get_state(sock_id) == SocketState::CONNECTED) {
            // Send data if writable
            if (socket_mgr.is_writable(sock_id)) {
                std::string request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
                socket_mgr.send(sock_id, 
                    std::vector<uint8_t>(request.begin(), request.end()));
            }
            
            // Receive data if readable
            if (socket_mgr.is_readable(sock_id)) {
                auto data = socket_mgr.receive(sock_id);
                if (data) {
                    std::cout << "Received " << data->size() << " bytes\n";
                    // Process data...
                }
            }
        }
        
        // Check for errors
        if (socket_mgr.has_error(sock_id)) {
            std::cerr << "Socket error: " 
                      << socket_mgr.get_error(sock_id) << "\n";
            socket_mgr.destroy_socket(sock_id);
        }
        
        // Check signals
        for (auto& sig : signal_mgr.poll_signals()) {
            if (sig.signo == SIGINT || sig.signo == SIGTERM) {
                std::cout << "Shutting down...\n";
                running = false;
            }
        }
        signal_mgr.clear_signals();
    }
    
    return 0;
}
```

## Directory Structure

```
poll-event-loop/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── api_reference.md
│   ├── design_decisions.md
│   └── performance_guide.md
├── include/
│   ├── event_loop.h
│   ├── timer_manager.h
│   ├── socket_manager.h
│   └── signal_manager.h
├── src/
│   ├── event_loop.cpp
│   ├── timer_manager.cpp
│   ├── socket_manager.cpp
│   └── signal_manager.cpp
├── tests/
│   ├── test_event_loop.cpp
│   ├── test_timer_manager.cpp
│   ├── test_socket_manager.cpp
│   └── test_signal_manager.cpp
├── examples/
│   ├── game_loop.cpp
│   ├── http_client.cpp
│   ├── echo_server.cpp
│   ├── chat_server.cpp
│   └── multi_timer.cpp
└── benchmarks/
    ├── timer_accuracy.cpp
    ├── socket_throughput.cpp
    └── poll_overhead.cpp
```

## Testing Strategy

### 1. Unit Tests (Google Test)
- Test each manager in isolation
- Mock EventLoop for manager testing
- Test edge cases (fd exhaustion, buffer overflow, etc.)
- Test error conditions

### 2. Integration Tests
- Multiple managers working together
- Real socket connections (localhost)
- Timer + socket coordination
- Signal handling during operations

### 3. Stress Tests
- 1000+ concurrent sockets
- 100+ timers firing simultaneously
- Sustained high throughput
- Memory leak detection (valgrind)

### 4. Real-world Tests
- HTTP client/server implementation
- Chat server with multiple clients
- File transfer protocol
- Load balancer example

### 5. Performance Tests (Google Benchmark)
- Poll overhead measurement
- Compare with epoll on Linux
- Latency measurements (p50, p99, p999)
- Throughput benchmarks

## Build System (CMake)

```cmake
cmake_minimum_required(VERSION 3.15)
project(PollEventLoop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library
add_library(poll_event_loop STATIC
    src/event_loop.cpp
    src/timer_manager.cpp
    src/socket_manager.cpp
    src/signal_manager.cpp
)

target_include_directories(poll_event_loop PUBLIC include)

# Examples
add_executable(game_loop examples/game_loop.cpp)
target_link_libraries(game_loop poll_event_loop)

# Tests (if GTest is available)
find_package(GTest)
if(GTest_FOUND)
    enable_testing()
    add_executable(tests
        tests/test_event_loop.cpp
        tests/test_timer_manager.cpp
        tests/test_socket_manager.cpp
    )
    target_link_libraries(tests poll_event_loop GTest::GTest GTest::Main)
    add_test(NAME all_tests COMMAND tests)
endif()
```

## Platform Support

### Initial Target: Linux
- `poll()` syscall for event multiplexing
- `timerfd_create()` for timers
- `signalfd()` for signal handling
- Non-blocking sockets with `O_NONBLOCK`

### Future Platforms

**BSD/macOS:**
- Replace poll() with kqueue
- Use kevent for timers (EVFILT_TIMER)
- Use kevent for signals (EVFILT_SIGNAL)

**Windows:**
- Replace poll() with select() or WSAPoll()
- Alternative timer mechanism (timeSetEvent or CreateWaitableTimer)
- Different signal handling approach

## Performance Considerations

### poll() Limitations
- O(n) scan of all file descriptors
- Efficient for <1000 FDs
- For >1000 FDs, consider epoll variant

### Optimization Opportunities
- Reserve buffer sizes to avoid reallocations
- Batch small writes together
- Use scatter/gather I/O for large transfers
- Profile hot paths with perf

### Memory Management
- Pre-allocate buffers based on expected usage
- Implement buffer pooling for high-frequency operations
- Monitor heap fragmentation
- Consider arena allocators for managers

## Error Handling Strategy

### Socket Errors
- ECONNREFUSED: Connection refused, report to user
- EPIPE: Broken pipe, close socket gracefully
- EAGAIN/EWOULDBLOCK: Not an error, normal for non-blocking
- EINTR: Interrupted syscall, retry

### Timer Errors
- EINVAL: Invalid timer parameters, assert/abort
- EMFILE: Too many open files, report to user

### Signal Errors
- Blocked signals: Document required signal setup
- Invalid signal numbers: Validate at add_signal()

## Documentation Requirements

### API Documentation
- Doxygen comments for all public methods
- Usage examples for each class
- Common pitfalls and solutions

### Design Documentation
- Architecture diagrams
- State machine diagrams for sockets
- Sequence diagrams for typical operations

### User Guide
- Getting started tutorial
- Migration guide from callback-based systems
- Performance tuning guide

## Success Criteria

1. **Functionality**: All managers work correctly in isolation and together
2. **Performance**: <1ms latency for event processing at 1000 events/sec
3. **Stability**: 24-hour stress test without crashes or memory leaks
4. **Usability**: Complete examples compile and run
5. **Documentation**: 100% API coverage, user guide complete

## Timeline Summary

- **Week 1**: EventLoop core + Timer manager foundations
- **Week 2**: Complete Timer manager + Socket manager basics
- **Week 3**: TCP client/server implementation
- **Week 4**: UDP + Signal manager + error handling
- **Week 5**: Examples, documentation, benchmarks
- **Week 6**: Polish, optimization, release prep

## Future Enhancements

### Post-v1.0 Features
- epoll backend for Linux (performance)
- kqueue backend for BSD/macOS
- IOCP backend for Windows
- File I/O manager (AIO or io_uring)
- DNS resolver (async)
- SSL/TLS support layer
- Buffer pool allocator
- Serialization helpers

### Advanced Features
- Multi-threaded event loop (work stealing)
- Proactor pattern variant
- Co-routine integration (C++20)
- Zero-copy optimization paths

## References

- `man 2 poll` - poll syscall documentation
- `man 2 timerfd_create` - timerfd documentation
- `man 2 signalfd` - signalfd documentation
- Stevens, "UNIX Network Programming" - Socket programming bible
- "Game Engine Architecture" - Game loop patterns
- Boost.Asio documentation - Comparison reference
