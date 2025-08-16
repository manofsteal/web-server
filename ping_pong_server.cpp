#include "poller.hpp"
#include "log.hpp"
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
    LOG_ERROR("Failed to create listener");
    return 1;
  }

  // Listen on port 8080
  if (!listener->start(8080)) {
    LOG_ERROR("Failed to listen on port 8080");
    return 1;
  }

  LOG("Ping-pong server listening on port 8080...");

  // Handle new connections
  listener->onAccept = [&ping_counter](Socket *client) {
    LOG("New connection from ", client->remote_addr, ":", client->remote_port);

    // Handle incoming data from client
    client->onData = [&ping_counter](Socket &socket, const BufferView &data) {
      std::string message(data.data, data.size);
      LOG("Received: ", message);

      if (message.find("ping") != std::string::npos) {
        ping_counter++;
        LOG("Sending pong response #", ping_counter);
        socket.write("pong " + std::to_string(ping_counter) + "\n");
      }
    };
  };

  LOG("Server running... (Press Ctrl+C to stop)");

  // Run the event loop forever
  poller.start();

  return 0;
}