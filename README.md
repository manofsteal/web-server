# C++ Web Server

A high-performance, cross-platform C++ web server built with an event-driven architecture using poll-based I/O multiplexing. The server supports sockets, listeners, and timers with platform-specific optimizations for Linux, macOS, and QNX.

## Features

- **Event-driven architecture** with poll-based I/O multiplexing
- **Cross-platform timer support** (Linux timerfd, macOS kqueue, QNX pulses)
- **Socket management** with automatic connection handling
- **Listener support** for accepting incoming connections
- **Resource pooling** for efficient memory management
- **Non-blocking I/O** operations
- **WebSocket client** with full RFC 6455 compliance
- **HTTP client** for making HTTP requests
- **Sequence execution** with pause/resume support
- **SteadyClock timing utilities** for consistent time measurements
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
- OpenSSL library (for WebSocket support)

### Installing websocat (optional, for performance comparison)

```bash
# Download websocat binary
curl -L https://github.com/vi/websocat/releases/latest/download/websocat.x86_64-unknown-linux-musl -o websocat
chmod +x websocat
```

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
- `websocket_client_example` - Basic WebSocket client demo connecting to echo server
- `websocket_client_example_2` - Advanced WebSocket client with multiple message types and timing
- `http_server_example` - HTTP server with file serving capabilities
- `http_client_example_2` - HTTP client making requests
- `sequence_example` - Sequential task execution demonstration
- `sequence_resumable_example` - Pausable/resumable sequence execution

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

## WebSocket Client

The project includes a fully functional WebSocket client that implements RFC 6455 WebSocket protocol.

### WebSocket Features

- **Full RFC 6455 compliance** - Complete WebSocket protocol implementation
- **Text and binary messages** - Support for both message types
- **Automatic ping/pong handling** - Built-in keepalive mechanism
- **Proper handshake** - Sec-WebSocket-Key generation and validation
- **Frame masking** - Client-to-server frame masking as required by spec
- **Connection lifecycle management** - Open, message, close event handling
- **Error handling** - Comprehensive error reporting

### WebSocket Usage

```cpp
#include "poller.hpp"
#include "websocket_client.hpp"
#include <iostream>

int main() {
    Poller poller;
    
    // Create WebSocket client
    WebSocketClient* client = WebSocketClient::fromSocket(poller.createSocket());
    
    // Set up event handlers
    client->onOpen = []() {
        std::cout << "WebSocket connection opened!" << std::endl;
    };
    
    client->onMessage = [](const std::string& message) {
        std::cout << "Received: " << message << std::endl;
    };
    
    client->onBinary = [](const std::vector<uint8_t>& data) {
        std::cout << "Received binary data: " << data.size() << " bytes" << std::endl;
    };
    
    client->onClose = [](uint16_t code, const std::string& reason) {
        std::cout << "Connection closed: " << code << " - " << reason << std::endl;
    };
    
    client->onError = [](const std::string& error) {
        std::cerr << "WebSocket error: " << error << std::endl;
    };
    
    // Connect to WebSocket server
    if (!client->connect("ws://echo.websocket.org/")) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Set up timer to send messages after connection
    poller.setTimeout(1000, [client]() {
        if (client->status == WebSocketStatus::OPEN) {
            client->sendText("Hello, WebSocket!");
            
            // Send binary data
            std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
            client->sendBinary(binary_data);
        }
    });
    
    // Run event loop
    poller.start();
    return 0;
}
```

### Testing WebSocket Client

1. **Build the WebSocket clients:**
   ```bash
   cd build
   make websocket_client_example
   make websocket_client_example_2
   ```

2. **Set up a test server (Python):**
   ```bash
   # Create virtual environment
   python3 -m venv websocket_test_env
   source websocket_test_env/bin/activate
   pip install websockets
   
   # Run the included echo server
   python websocket_echo_server.py
   ```

3. **Run the WebSocket server (C++ implementation):**
   ```bash
   ./websocket_server_example
   ```

   **Or run websocat echo server (Rust reference implementation):**

   curl -L https://github.com/vi/websocat/releases/latest/download/websocat.x86_64-unknown-linux-musl -o websocat
   
   ```bash
   ./websocat ws-l:0.0.0.0:8765 mirror: --text
   ```

4. **Run the clients:**
   ```bash
   # Basic example
   ./websocket_client_example
   
   # Advanced example with multiple message types
   ./websocket_client_example_2
   ```

5. **Expected output:**
   ```
   WebSocket Client Example
   ========================
   [WebSocket] Starting connection to: ws://localhost:8765/
   [WebSocket] Socket connected successfully
   [WebSocket] Performing handshake
   WebSocket connection opened!
   [WebSocket] Handshake successful, connection is now OPEN
   Sending test messages...
   Received message: Echo: Hello, WebSocket!
   Received message: Echo: This is a test message
   Received message: Echo: Goodbye!
   Received message: Echo: b'Hello'
   WebSocket closed: 1000 - Normal closure
   ```

   **Advanced Example Output:**
   ```
   WebSocket Client Example 2 - Advanced Features
   ===============================================
   🚀 Connecting to WebSocket server...
   🔗 WebSocket connection established!
   🎯 Starting advanced messaging test...
   📨 [1003ms] Text message #1: Echo: {"type":"greeting","message":"Hello WebSocket!","timestamp":3607633609}
   📨 [1504ms] Text message #2: Echo: Special chars: 🚀 ñáéíóú αβγ ∑∞ 中文 русский
   📨 [2405ms] Text message #3: Echo: b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\t'
   📨 [2405ms] Text message #4: Echo: b'\xaa\xab\xac\xad\xae\xaf\xb0\xb1'
   📨 [2406ms] Text message #5: Echo: b'\x89PNG\r\n\x1a\n'
   ✅ Test sequence completed. Closing connection...
   🔒 [5408ms] Connection closed with code: 1000, reason: Normal test completion
   📊 Session stats - Sent: 7, Received: 5
   🏁 WebSocket Client Example 2 completed in 6408ms
   ```

### WebSocket API

#### WebSocketClient Methods
- `bool connect(const std::string& url)` - Connect to WebSocket server
- `void sendText(const std::string& message)` - Send text message
- `void sendBinary(const std::vector<uint8_t>& data)` - Send binary data
- `void close(uint16_t code = 1000, const std::string& reason = "")` - Close connection

#### WebSocket Event Callbacks
- `onOpen()` - Called when connection is established
- `onMessage(const std::string& message)` - Called when text message received
- `onBinary(const std::vector<uint8_t>& data)` - Called when binary data received
- `onClose(uint16_t code, const std::string& reason)` - Called when connection closes
- `onError(const std::string& error)` - Called when error occurs

#### Supported URL Formats
- `ws://hostname:port/path` - Standard WebSocket connection
- `wss://hostname:port/path` - Secure WebSocket (if SSL support enabled)
- Default ports: 80 for ws://, 443 for wss://

## Timing Utilities

### SteadyClock

A wrapper around `std::chrono::steady_clock` providing convenient timing methods:

```cpp
#include "steady_clock.hpp"

// Get current time
auto start = SteadyClock::now();

// Add milliseconds to timepoint
auto future = SteadyClock::addMilliseconds(start, 1000);

// Calculate elapsed time
int elapsed = SteadyClock::elapsedMs(start);

// Calculate duration between two timepoints
int duration = SteadyClock::durationMs(start, future);
```

### SteadyTimer

A simple timer class for measuring elapsed time:

```cpp
#include "steady_timer.hpp"

SteadyTimer timer;

// Reset timer to current time
timer.reset();

// Check if specific time has elapsed
if (timer.isExpiredMs(1000)) {
    std::cout << "1 second has passed!" << std::endl;
}

// Get elapsed milliseconds
int elapsed = timer.getElapsedMs();
```

## Sequence Execution

The sequence system allows for sequential execution of tasks with timing control:

```cpp
#include "sequence.hpp"

Poller poller;
Sequence sequence(poller);

// Add tasks to sequence
sequence.addTask([]() {
    std::cout << "Task 1 executed" << std::endl;
});

sequence.addWait(1000); // Wait 1 second

sequence.addTask([]() {
    std::cout << "Task 2 executed" << std::endl;
});

// Wait for condition with timeout
sequence.addWait([]() -> bool {
    return some_condition_met();
}, 100, 5000); // Check every 100ms, timeout after 5s

// Start sequence execution
sequence.start();
poller.start();

// Pause/resume support
sequence.pause();  // Pause at current position
sequence.resume(); // Resume from where paused
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