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

  std::cout << "Socket created with ID: " << socket->id << std::endl;

  // Connect to server
  if (!socket->start("127.0.0.1", 8080)) {
    std::cerr << "Failed to connect to server" << std::endl;
    return 1;
  }

  std::cout << "Connected to ping-pong server!" << std::endl;

  // Handle server responses
  socket->onData = [](Socket &socket, const Buffer &data) {
    std::string response;
    for (size_t i = 0; i < data.size(); ++i) {
      response += data.getAt(i);
    }
    std::cout << "Server response: " << response;
  };

  // Create timer to send ping every second
  Timer *timer = poller.createTimer();
  if (!timer) {
    std::cerr << "Failed to create timer" << std::endl;
    return 1;
  }

  std::cout << "Timer created with ID: " << timer->id << std::endl;

  // Send ping every 1000ms (1 second)
  if (!timer->setInterval(1000, [socket]() {
        std::cout << "Timer fired! Sending ping..." << std::endl;
        socket->write("ping\n");
      })) {
    std::cerr << "Failed to start timer" << std::endl;
    return 1;
  }

  std::cout << "Timer started successfully!" << std::endl;
  std::cout << "Client running... (Press Ctrl+C to stop)" << std::endl;

  // Run the event loop forever
  poller.start();

  return 0;
}