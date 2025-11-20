# NetworkSystem Test

This test demonstrates the simplified `NetworkSystem` API in three modes.

## Usage

### 1. Integrated Test (Default)
Runs server and client in a single process:
```bash
./network_system_test
```

### 2. Server Mode
Runs as an echo server on port 8085:
```bash
./network_system_test server
```
The server will:
- Accept connections
- Send "Welcome from server" to new clients
- Echo back any received messages with "Echo: " prefix

### 3. Client Mode
Connects to server on port 8085 and validates echo:
```bash
./network_system_test client
```
The client will:
- Connect to 127.0.0.1:8085
- Send "Hello from client"
- Validate echo response is "Echo: Hello from client"
- Exit with success/failure

## Testing Across Processes

Terminal 1 (Server):
```bash
./network_system_test server
```

Terminal 2 (Client):
```bash
./network_system_test client
```

## Features Demonstrated

- **Simple API**: Single `NetworkSystem` object
- **Auto-registration**: Factory methods handle manager registration
- **Unified events**: Single `NetworkEvent` type
- **Time-based loops**: Uses timers instead of frame counting
- **Clean separation**: Server/client can run independently
