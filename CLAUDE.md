# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build all targets
cd build && cmake .. && make

# Build specific test
make <test_name>    # e.g., make socket_test, make network_system_test

# Run a specific test
./build/<test_name>

# Common test targets
make network_system_test    # NetworkSystem API tests
make websocket_client_test  # WebSocket client tests
make http_server_test       # HTTP server tests
make socket_test           # Basic socket tests
make game_loop_test        # Game loop/timer tests
```

## Architecture Overview

### Core Design Philosophy

This is a **Reactor-pattern** network library (not Proactor like ASIO). The design explicitly avoids:
- Virtual functions and inheritance
- Exceptions (uses return codes)
- Smart pointers (manual memory management)
- Class encapsulation (public struct members)
- RAII patterns
- Thread safety primitives

See `reactor_design.md` for detailed architectural rationale and performance analysis.

### Key Components

**Poller** (`include/websrv/poller.hpp`)
- Central event loop using `poll()` system call
- Manages pollable objects (sockets, listeners, timers)
- Factory pattern: `createSocket()`, `createListener()`, `createTimer()`
- Returns events as `std::vector<PollerEvent>` for user processing

**Socket** (`include/websrv/socket.hpp`)
- Inherits from `Pollable` base struct
- Buffer-based I/O using `Buffer*` queue system
- Read buffers: `read()` returns ownership, `getReadBuffer()` for access
- Write buffers: `write(Buffer*)` queues data, `getWriteBuffer()` for zero-copy
- Methods: `handleRead()`, `handleWrite()`, `handleError()`

**Buffer** (`include/websrv/buffer.hpp`)
- Thin wrapper around `std::vector<uint8_t>`
- Methods: `append()`, `consume()`, `data()`, `size()`
- Used throughout for all I/O operations

**NetworkSystem** (`include/websrv/network_system.hpp`)
- High-level unified API wrapping Poller + Managers
- Factory methods auto-register with managers
- Returns unified `NetworkEvent` struct (ACCEPTED, SOCKET_DATA, SOCKET_CLOSED, SOCKET_ERROR)
- Simplifies common use cases while allowing access to underlying components

**Managers** (`include/websrv/listener_manager.hpp`, `socket_manager.hpp`)
- ListenerManager: handles accept() and new connection creation
- SocketManager: handles Socket I/O events and lifecycle
- Both use manager pattern to centralize logic

### Platform-Specific Features

**Timers** (cross-platform via Poller)
- Linux: `timerfd_create()` for high-precision
- macOS: `kqueue` with `EVFILT_TIMER`
- QNX: pulse-based timers
- API: `createTimer(delay_ms, repeat)`, `isTimerExpired(id)`

### Protocol Implementations

**HTTP Server** (`include/websrv/http_server.hpp`)
- Supports WebSocket upgrade via Upgrade header
- File serving capabilities
- Request/response handling

**WebSocket** (`include/websrv/websocket_client.hpp`, `websocket_server.hpp`)
- RFC 6455 compliant
- Client: handshake, masking, ping/pong
- Server: accept handshake, handle frames
- Requires OpenSSL for SHA-1 in handshake

**HTTP Client** (`include/websrv/http_client.hpp`)
- Basic HTTP request/response handling

## Code Style Requirements (from .rules)

**Strict Coding Rules**:
- Use `struct`, never `class`
- All members must be public
- No virtual functions or inheritance (except base types like Pollable)
- No constructors (use struct initialization)
- No exceptions (return bool/error codes)
- No smart pointers
- Function definitions in `.cpp` files ONLY, declarations in `.hpp`

**Naming Conventions**:
- Structs: PascalCase (e.g., `Socket`, `Poller`)
- Members: snake_case (e.g., `remote_addr`, `pending_read_buffers`)
- Methods: camelCase (e.g., `handleRead()`, `createSocket()`)
- Constants: UPPER_SNAKE_CASE

**Memory Management**:
- Manual allocation/deallocation
- Clear ownership semantics
- Explicit cleanup methods

**File Organization**:
- One struct per header file
- Use `#pragma once`
- Forward declarations to avoid circular dependencies
- Include headers only in `.cpp` when possible

**Thread Safety**:
- Single-threaded design
- No mutexes, atomics, or synchronization primitives
- No concurrent access protection

## Common Usage Patterns

### NetworkSystem API (Recommended for new code)

```cpp
NetworkSystem network;

// Create server
Listener* server = network.createListener(8080);

// Create client
Socket* client = network.createSocket("127.0.0.1", 8080);

// Main loop
while (true) {
    auto events = network.poll(100);  // 100ms timeout

    for (const auto& event : events) {
        switch (event.type) {
            case NetworkEvent::ACCEPTED:
                event.socket->write(toBuffer("Welcome"));
                break;
            case NetworkEvent::SOCKET_DATA:
                std::string msg = fromBuffer(event.socket->read());
                event.socket->write(toBuffer("Echo: " + msg));
                break;
            case NetworkEvent::SOCKET_CLOSED:
                // Handle close
                break;
            case NetworkEvent::SOCKET_ERROR:
                // Handle error
                break;
        }
    }

    network.removeClosedSockets(events);
}
```

### Direct Poller API (Maximum control)

```cpp
Poller poller;
Socket* socket = poller.createSocket();

while (true) {
    auto events = poller.poll(100);

    for (const auto& pe : events) {
        Pollable* pollable = poller.getPollable(pe.id);
        Socket* sock = static_cast<Socket*>(pollable);

        if (pe.revents & POLLIN) {
            sock->handleRead();
            auto buffers = sock->read();
            // Process buffers
        }
        if (pe.revents & POLLOUT) {
            sock->handleWrite();
        }
    }
}
```

### Buffer Helpers

Utility functions exist (see test files):
- `toBuffer(const std::string& str)` - Convert string to Buffer*
- `fromBuffer(std::vector<Buffer*>)` - Convert buffers to string

## Testing

Tests demonstrate usage patterns:
- `network_system_test.cpp`: NetworkSystem API example (client/server)
- `websocket_client_test.cpp`: WebSocket client usage
- `http_server_test.cpp`: HTTP server with file serving
- `socket_test.cpp`: Low-level Socket API
- `game_loop_test.cpp`: Timer and game loop patterns

Run tests with optional modes:
```bash
./network_system_test          # Integrated test
./network_system_test server   # Server mode
./network_system_test client   # Client mode
```

## Important Architectural Notes

1. **User Controls Event Loop**: Unlike ASIO, the user explicitly writes the main loop and calls `poll()`

2. **Ownership Model**:
   - Poller owns Pollable objects (sockets, listeners)
   - Buffer ownership transfers via `read()` (caller owns returned buffers)
   - Write buffers owned by Socket until transmitted

3. **No Callbacks**: Events returned as data structures, user processes explicitly

4. **Error Handling**:
   - Return `bool` for success/failure
   - Return `nullptr` on factory method failure
   - Check return values explicitly

5. **Zero-Copy Opportunities**:
   - `getWriteBuffer()` returns buffer for direct writing
   - Avoid copying when possible

## When Working on This Codebase

- Read `reactor_design.md` to understand the "why" behind design decisions
- Follow `.rules` strictly - violations will break the design philosophy
- Use `NetworkSystem` for new high-level code
- Use `Poller` directly only when you need fine control
- Never add virtual functions or class hierarchies
- Test patterns are in the `test/` directory - reference them
- Remember: explicit control flow over implicit abstraction
