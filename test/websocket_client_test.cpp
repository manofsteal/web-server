#include "websrv/log.hpp"
#include "websrv/poller.hpp"
#include "websrv/websocket_client.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace websrv;

int main() {
  Poller poller;

  // Create WebSocket client
  WebSocketClient *client = WebSocketClient::fromSocket(poller.createSocket());

  if (!client) {
    LOG_ERROR("Failed to create WebSocket client");
    return 1;
  }

  LOG("WebSocket Client Example");
  LOG("========================");

  // Set up event handlers
  client->onOpen = []() { LOG("WebSocket connection opened!"); };

  client->onMessage = [](const std::string &message) {
    LOG("Received message: ", message);
  };

  client->onBinary = [](const std::vector<uint8_t> &data) {
    LOG("Received binary data: ", data.size(), " bytes");
  };

  client->onClose = [](uint16_t code, const std::string &reason) {
    LOG("WebSocket closed: ", code, " - ", reason);
  };

  client->onError = [](const std::string &error) {
    LOG_ERROR("WebSocket error: ", error);
  };

  // Connect to local WebSocket echo server
  bool success = client->connect("ws://localhost:8765/echo");
  // bool success = client->connect("ws://localhost:8765");

  if (!success) {
    LOG_ERROR("Failed to connect to WebSocket server");
    return 1;
  }

  // Set up a timer to send test messages after connection is established
  poller.setTimeout(2000, [client, &poller]() {
    if (client->status == WebSocketStatus::OPEN) {
      LOG("Sending test messages...");
      client->sendText("Hello, WebSocket!");

      poller.setTimeout(1000, [client, &poller]() {
        client->sendText("This is a test message");

        poller.setTimeout(1000, [client, &poller]() {
          client->sendText("Goodbye!");

          // Send binary data
          std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C,
                                              0x6F}; // "Hello"
          client->sendBinary(binary_data);

          poller.setTimeout(1000, [client, &poller]() {
            client->close(1000, "Normal closure");
            poller.setTimeout(1000, [&poller]() { poller.stop(); });
          });
        });
      });
    } else {
      LOG("WebSocket not open, status: ", (int)client->status);
      poller.stop();
    }
  });

  // Run the event loop
  poller.start();

  LOG("WebSocket Client Example completed.");

  return 0;
}