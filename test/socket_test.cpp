#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace websrv;

int main() {
  Poller poller;

  // Create a socket using the poller factory (automatically added to poller)
  Socket *socket = poller.createSocket();
  if (!socket) {
    LOG_ERROR("Failed to create socket");
    return 1;
  }

  LOG("Socket created successfully!");
  LOG("Socket ID: ", socket->id);
  LOG("Socket file descriptor: ", socket->file_descriptor);

  // For now, just demonstrate that the socket was created
  // In a real example, you would connect to a server or bind/listen

  // Run for a short time to demonstrate the poller works
  std::thread run_thread([&poller]() { poller.start(); });

  std::this_thread::sleep_for(std::chrono::seconds(2));
  poller.stop();
  run_thread.join();

  LOG("Socket example completed successfully!");

  return 0;
}