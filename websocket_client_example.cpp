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

  // Connect to WebSocket echo server
  // Using echo.websocket.org IP address directly
  bool success = client->connect("echo.websocket.org:80");

  if (!success) {
    std::cerr << "Failed to connect to WebSocket server" << std::endl;
    return 1;
  }

  // Run the event loop
  poller.start();

  // Wait a bit for connection to establish
  // std::this_thread::sleep_for(std::chrono::seconds(5));

  // std::cout << "WebSocket status: " << (int)client->status << std::endl;

  // // Send some test messages
  // if (client->status == WebSocketStatus::OPEN) {
  //   std::cout << "Sending test messages..." << std::endl;

  //   client->send("Hello, WebSocket!");
  //   std::this_thread::sleep_for(std::chrono::milliseconds(500));

  //   client->send("This is a test message");
  //   std::this_thread::sleep_for(std::chrono::milliseconds(500));

  //   client->send("Goodbye!");
  //   std::this_thread::sleep_for(std::chrono::milliseconds(500));

  //   // Send binary data
  //   std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6C, 0x6C,
  //                                       0x6F}; // "Hello"
  //   client->sendBinary(binary_data);
  //   std::this_thread::sleep_for(std::chrono::milliseconds(500));

  //   // Close the connection
  //   client->close(1000, "Normal closure");
  // }

  // // Wait for responses
  // std::this_thread::sleep_for(std::chrono::seconds(3));

  // poller.stop();
  // run_thread.join();

  std::cout << "WebSocket Client Example completed." << std::endl;

  return 0;
}