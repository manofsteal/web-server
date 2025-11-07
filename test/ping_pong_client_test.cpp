#include "websrv/poller.hpp"
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
  socket->onData = [](Socket &socket, const BufferView &data) {
    std::string response(data.data, data.size);
    std::cout << "Server response: " << response;
  };

  // Create timer to send ping every second using new API
  Poller::TimerID timerId = poller.setInterval(1000, [socket]() {
    std::cout << "Timer fired! Sending ping..." << std::endl;
    socket->write("ping\n");
  });

  std::cout << "Timer created with ID: " << timerId << std::endl;
  std::cout << "Timer started successfully!" << std::endl;
  std::cout << "Client running... (Press Ctrl+C to stop)" << std::endl;

  // Run the event loop forever
  poller.start();

  return 0;
}