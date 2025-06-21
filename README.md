# C++ Web Server

A high-performance, cross-platform C++ web server built with an event-driven architecture using poll-based I/O multiplexing. The server supports sockets, listeners, and timers with platform-specific optimizations for Linux, macOS, and QNX.

## Features

- **Event-driven architecture** with poll-based I/O multiplexing
- **Cross-platform timer support** (Linux timerfd, macOS kqueue, QNX pulses)
- **Socket management** with automatic connection handling
- **Listener support** for accepting incoming connections
- **Resource pooling** for efficient memory management
- **Non-blocking I/O** operations
- **Ping-pong example** demonstrating client-server communication

## Architecture

The web server is built around several core components:

### Core Components

- **Poller**: Central event loop that manages all I/O operations using poll()
- **Pollable**: Base structure for all pollable objects (sockets, listeners, timers)
- **Socket**: TCP socket wrapper with callback-based data handling
- **Listener**: Server socket for accepting incoming connections
- **Timer**: Cross-platform timer implementation with timeout/interval support
- **PollablePoolManager**: Resource pool manager for efficient object reuse

### Platform-Specific Timers

- **Linux**: Uses `timerfd_create()` for high-precision timers
- **macOS**: Uses `kqueue` with `EVFILT_TIMER` for efficient timer management
- **QNX**: Uses pulse-based timers with channels for real-time systems

## Building

### Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC, Clang, or MSVC)
- pthread library

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build all targets
make

# Or build specific targets
make ping_pong_server
make ping_pong_client
make timer_example
make socket_example
```

### Build Targets

- `ping_pong_server` - Example server that responds "pong" to "ping" messages
- `ping_pong_client` - Example client that sends "ping" every second
- `timer_example` - Demonstrates timer functionality
- `socket_example` - Basic socket creation example

## Usage Examples

### Basic Server

```cpp
#include "poller.hpp"
#include <iostream>

int main() {
    Poller poller;

    // Create and configure listener
    Listener* listener = poller.createListener();
    if (!listener->listen(8080)) {
        std::cerr << "Failed to listen on port 8080" << std::endl;
        return 1;
    }

    // Handle new connections
    listener->setOnAccept([](Socket* client) {
        std::cout << "New connection from " << client->remote_addr 
                  << ":" << client->remote_port << std::endl;

        // Handle incoming data
        client->setOnData([](Socket& socket, const std::vector<char>& data) {
            std::string message(data.begin(), data.end());
            std::cout << "Received: " << message;
            
            // Echo back to client
            socket.write("Echo: " + message);
        });
    });

    std::cout << "Server listening on port 8080..." << std::endl;
    poller.run();
    return 0;
}
```

### Basic Client

```cpp
#include "poller.hpp"
#include <iostream>

int main() {
    Poller poller;

    // Create and connect socket
    Socket* socket = poller.createSocket();
    if (!socket->connect("127.0.0.1", 8080)) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Handle server responses
    socket->setOnData([](Socket& socket, const std::vector<char>& data) {
        std::string response(data.begin(), data.end());
        std::cout << "Server response: " << response;
    });

    // Send a message
    socket->write("Hello, Server!\n");

    poller.run();
    return 0;
}
```

### Timer Usage

```cpp
#include "poller.hpp"
#include <iostream>

int main() {
    Poller poller;

    // Create timer
    Timer* timer = poller.createTimer();
    
    // Set timeout (one-time)
    timer->setTimeout(2000, [](Any* data) {
        std::cout << "Timer fired after 2 seconds!" << std::endl;
    });

    // Or set interval (repeating)
    Timer* intervalTimer = poller.createTimer();
    intervalTimer->setInterval(1000, [](Any* data) {
        std::cout << "Interval timer: " << std::time(nullptr) << std::endl;
    });

    poller.run();
    return 0;
}
```

## Running Examples

### Ping-Pong Demo

1. **Start the server:**
   ```bash
   ./ping_pong_server
   ```
   Server will listen on port 8080.

2. **Start the client:**
   ```bash
   ./ping_pong_client
   ```
   Client will connect and send "ping" every second, server responds with "pong".

3. **Expected output:**
   
   Server:
   ```
   Ping-pong server listening on port 8080...
   New connection from 127.0.0.1:54321
   Received: ping
   Sending pong response
   ```
   
   Client:
   ```
   Connected to ping-pong server!
   Sending ping...
   Server response: pong
   ```

### Timer Example

```bash
./timer_example
```

Expected output:
```
Timer fired!
```

### Socket Example

```bash
./socket_example
```

Expected output:
```
Socket created successfully!
Socket ID: 0
Socket file descriptor: 3
Socket example completed successfully!
```

## API Reference

### Poller

- `Socket* createSocket()` - Create a new socket
- `Timer* createTimer()` - Create a new timer
- `Listener* createListener()` - Create a new listener
- `void run()` - Start the event loop
- `void stop()` - Stop the event loop
- `void cleanup()` - Clean up all resources

### Socket

- `bool connect(const std::string& host, uint16_t port)` - Connect to remote host
- `void setOnData(Callback cb)` - Set data received callback
- `void write(const std::string& data)` - Send data to remote peer

### Listener

- `bool listen(uint16_t port)` - Start listening on port
- `void setOnAccept(AcceptCallback cb)` - Set new connection callback

### Timer

- `bool setTimeout(uint32_t ms, Callback cb)` - Set one-time timer
- `bool setInterval(uint32_t ms, Callback cb)` - Set repeating timer
- `void stop()` - Stop the timer

## Design Principles

This web server follows specific design principles for performance and maintainability:

- **No virtual functions** - Direct function calls for better performance
- **Struct-based design** - All members are public, no encapsulation overhead
- **Manual memory management** - Explicit control over resource allocation
- **C-style error handling** - Return values instead of exceptions
- **Platform-specific optimizations** - Native timer implementations for each OS

## Platform Support

- **Linux** - Full support with timerfd and epoll optimizations
- **macOS** - Full support with kqueue-based timers
- **QNX** - Real-time support with pulse-based messaging
- **Other POSIX systems** - Basic support with poll() fallback

## Contributing

When contributing to this project, please follow the coding standards defined in `.rules`:

- Use struct instead of class
- No virtual functions or inheritance
- Manual memory management
- C-style error handling
- Function definitions in .cpp files only

## License

This project is open source. Please refer to the license file for details. 


### TODO


#
## Performance Optimizations

The server uses vectored I/O operations for improved performance:

- **readv()** - Scatter input: read data into multiple buffers in a single system call
- **writev()** - Gather output: write data from multiple buffers in a single system call

These operations reduce the number of system calls and improve throughput by allowing the kernel to handle multiple buffer operations atomically.

### Vectored I/O API