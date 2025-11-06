#include "listener.hpp"
#include "log.hpp"
#include "poller.hpp"
#include "websocket_server.hpp"
#include <chrono>
#include <iostream>
#include <signal.h>
#include <thread>

// can use websocket_client_test.py to test this

int main() {

  LOG("Starting WebSocket echo server on port 8765");

  // Create poller and listener
  Poller poller;
  Listener *listener = poller.createListener();
  if (!listener) {
    LOG_ERROR("Failed to create listener");
    return 1;
  }

  // Start listening on port 8765 (same as Python echo server)
  if (!listener->start(8765)) {
    LOG_ERROR("Failed to listen on port 8765");
    return 1;
  }

  // Create WebSocket server
  WebSocketServer server(listener);

  // Set up connection handlers
  server.onConnection = [](WebSocketConnection &conn) {
    LOG("[Server] New WebSocket connection established for path: ", conn.path);
  };

  server.onDisconnection = [](WebSocketConnection &conn) {
    LOG("[Server] WebSocket connection closed for path: ", conn.path);
  };

  // Route for echo functionality
  server.route("/", [](WebSocketConnection &conn) { // echo
    LOG("[Echo Route] Setting up echo handlers for connection");

    conn.onMessage = [](WebSocketConnection &connection,
                        const std::string &message) {
      LOG("[Echo] Received text message: ", message);
      connection.sendText(message);
    };

    conn.onBinary = [](WebSocketConnection &connection,
                       const std::vector<uint8_t> &data) {
      LOG("[Echo] Received binary message of size: ", data.size());
      connection.sendBinary(data);
    };
  });

  // Route for chat functionality
  server.route("/chat", [](WebSocketConnection &conn) {
    LOG("[Chat Route] Setting up chat handlers for connection");

    conn.onMessage = [](WebSocketConnection &connection,
                        const std::string &message) {
      LOG("[Chat] Received message: ", message);
      connection.sendText("Chat response: " + message);
    };
  });

  LOG("WebSocket server started. Routes available:");
  LOG("  - ws://localhost:8765/ - Echo server");
  LOG("  - ws://localhost:8765/chat - Chat server");
  LOG("Press Ctrl+C to stop");

  // Start poller
  poller.start();

  return 0;
}