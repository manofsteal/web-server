#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main() {
  Poller poller;
  int ping_counter = 0;

  // Create and configure listener
  Listener *listener = poller.createListener();
  if (!listener) {
    std::cerr << "Failed to create listener" << std::endl;
    return 1;
  }

  // Listen on port 8080
  if (!listener->start(8080)) {
    std::cerr << "Failed to listen on port 8080" << std::endl;
    return 1;
  }

  std::cout << "Ping-pong server listening on port 8080..." << std::endl;

  // Handle new connections
  listener->onAccept = [&ping_counter](Socket *client) {
    std::cout << "New connection from " << client->remote_addr << ":"
              << client->remote_port << std::endl;

    // Handle incoming data from client
    client->onData = [&ping_counter](Socket &socket, const BufferView &data) {
      std::string message(data.data, data.size);
      std::cout << "Received: " << message;

      if (message.find("ping") != std::string::npos) {
        ping_counter++;
        std::cout << "Sending pong response #" << ping_counter << std::endl;
        socket.write("pong " + std::to_string(ping_counter) + "\n");
      }
    };
  };

  std::cout << "Server running... (Press Ctrl+C to stop)" << std::endl;

  // Run the event loop forever
  poller.start();

  return 0;
}