#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  Poller poller;

  // Create timers using the poller factory
  Timer *intervalTimer = poller.createTimer();
  Timer *timeoutTimer = poller.createTimer();

  if (!intervalTimer || !timeoutTimer) {
    std::cerr << "Failed to create timers\n";
    return 1;
  }

  // Counter to track interval timer firings
  static int intervalCounter = 0;

  // Set up interval timer (fires every 1 second)
  if (!intervalTimer->setInterval(1000, [](Any *data) {
        intervalCounter++;
        std::cout << "Interval timer fired! Count: " << intervalCounter
                  << std::endl;
      })) {
    std::cerr << "Failed to start interval timer" << std::endl;
    return 1;
  }

  // Set up timeout timer (fires once after 3 seconds)
  if (!timeoutTimer->setTimeout(3000, [](Any *data) {
        std::cout << "Timeout timer fired after 3 seconds!" << std::endl;
      })) {
    std::cerr << "Failed to start timeout timer\n";
    return 1;
  }

  std::cout << "Timers started:" << std::endl;
  std::cout << "- Interval timer: fires every 1 second" << std::endl;
  std::cout << "- Timeout timer: fires once after 3 seconds" << std::endl;
  std::cout << "Running for 6 seconds..." << std::endl;

  // Run for 6 seconds
  std::thread run_thread([&poller]() { poller.run(); });

  std::this_thread::sleep_for(std::chrono::seconds(6));
  poller.stop();
  run_thread.join();

  poller.cleanup();

  std::cout << "Timer test completed. Interval timer fired " << intervalCounter
            << " times." << std::endl;
  return 0;
}