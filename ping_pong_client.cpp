#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
  Poller poller;

  // Create socket and connect to server
  Socket *socket = poller.createSocket();
  if (!socket) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }

  // Connect to server
  if (!socket->connect("127.0.0.1", 8080)) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

  std::cout << "Connected to ping-pong server!" << std::endl;

  // Handle server responses
  socket->setOnData([](Socket &socket, const std::vector<char> &data) {
    std::string response(data.begin(), data.end());
    std::cout << "Server response: " << response;
  });

  // Create timer to send ping every second
  Timer *timer = poller.createTimer();
  if (!timer) {
    std::cerr << "Failed to create timer" << std::endl;
    return 1;
  }

  // Send ping every 1000ms (1 second)
  if (!timer->setInterval(1000, [socket](Any *data) {
        std::cout << "Sending ping..." << std::endl;
        socket->write("ping\n");
      })) {
    std::cerr << "Failed to start timer" << std::endl;
    return 1;
  }

  std::cout << "Client running... (Press Ctrl+C to stop)" << std::endl;

  // Run the event loop forever
  poller.run();

  return 0;
}