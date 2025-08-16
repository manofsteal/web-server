#include "poller.hpp"
#include "websocket_client.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  Poller poller;

  // Create WebSocket client
  WebSocketClient *client = WebSocketClient::fromSocket(poller.createSocket());

  if (!client) {
    std::cerr << "Failed to create WebSocket client" << std::endl;
    return 1;
  }

  std::cout << "WebSocket Client Example" << std::endl;
  std::cout << "========================" << std::endl;

  // Set up event handlers
  client->onOpen = []() {
    std::cout << "WebSocket connection opened!" << std::endl;
  };

  client->onMessage = [](const std::string &message) {
    std::cout << "Received message: " << message << std::endl;
  };

  client->onBinary = [](const std::vector<uint8_t> &data) {
    std::cout << "Received binary data: " << data.size() << " bytes"
              << std::endl;
  };

  client->onClose = [](uint16_t code, const std::string &reason) {
    std::cout << "WebSocket closed: " << code << " - " << reason << std::endl;
  };

  client->onError = [](const std::string &error) {
    std::cerr << "WebSocket error: " << error << std::endl;
  };

  // Connect to local WebSocket echo server
  bool success = client->connect("ws://localhost:8765/");

  if (!success) {
    std::cerr << "Failed to connect to WebSocket server" << std::endl;
    return 1;
  }

  // Set up a timer to send test messages after connection is established
  poller.setTimeout(2000, [client, &poller]() {
    if (client->status == WebSocketStatus::OPEN) {
      std::cout << "Sending test messages..." << std::endl;
      client->send("Hello, WebSocket!");
      
      poller.setTimeout(1000, [client, &poller]() {
        client->send("This is a test message");
        
        poller.setTimeout(1000, [client, &poller]() {
          client->send("Goodbye!");
          
          // Send binary data
          std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
          client->sendBinary(binary_data);
          
          poller.setTimeout(1000, [client, &poller]() {
            client->close(1000, "Normal closure");
            poller.setTimeout(1000, [&poller]() {
              poller.stop();
            });
          });
        });
      });
    } else {
      std::cout << "WebSocket not open, status: " << (int)client->status << std::endl;
      poller.stop();
    }
  });

  // Run the event loop
  poller.start();

  std::cout << "WebSocket Client Example completed." << std::endl;

  return 0;
}