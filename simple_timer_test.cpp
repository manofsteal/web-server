#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::cout << "Starting simple timer test..." << std::endl;

  Poller poller;

  // Create a simple timer
  Timer *timer = poller.createTimer();
  if (!timer) {
    std::cerr << "Failed to create timer" << std::endl;
    return 1;
  }

  std::cout << "Timer created with ID: " << timer->id << std::endl;
  std::cout << "Timer file descriptor: " << timer->file_descriptor << std::endl;

  // Set a simple timeout
  if (!timer->setTimeout(1000, [](Any *data) {
        std::cout << "Timer callback executed!" << std::endl;
      })) {
    std::cerr << "Failed to start timer" << std::endl;
    return 1;
  }

  std::cout << "Timer started, file descriptor after start: "
            << timer->file_descriptor << std::endl;
  std::cout << "Running poller for 3 seconds..." << std::endl;

  // Run for 3 seconds
  std::thread run_thread([&poller]() { poller.run(); });

  std::this_thread::sleep_for(std::chrono::seconds(3));
  poller.stop();
  run_thread.join();

  std::cout << "Test completed." << std::endl;
  return 0;
}