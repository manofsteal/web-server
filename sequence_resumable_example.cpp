#include "poller.hpp"
#include "sequence.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  Poller poller;
  Sequence sequence(poller);

  auto start_time = std::chrono::steady_clock::now();

  // Add tasks to test pause/resume
  sequence.addTask(
      [start_time]() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time);
        std::cout << "Task 1 executed at " << elapsed.count() << "ms"
                  << std::endl;
      },
      1000);

  sequence.addDelay(2000); // 2 second delay to test pause during delay

  sequence.addTask([start_time]() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    std::cout << "Task 2 executed at " << elapsed.count() << "ms" << std::endl;
  });

  sequence.addDelay(2000); // 2 second delay to test pause during delay

  sequence.addTask([&poller, start_time]() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    std::cout << "Task 3 executed at " << elapsed.count()
              << "ms - stopping poller" << std::endl;
    poller.stop();
  });

  std::cout << "Starting sequence..." << std::endl;
  sequence.start();

  // Test pause/resume in a separate thread
  std::thread test_thread([&sequence, start_time]() {
    // Pause after 2 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    auto pause_time = std::chrono::steady_clock::now();
    auto pause_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        pause_time - start_time);
    std::cout << "Pausing sequence at " << pause_elapsed.count() << "ms"
              << std::endl;
    sequence.pause();

    // Resume after 3 seconds of being paused
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    auto resume_time = std::chrono::steady_clock::now();
    auto resume_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        resume_time - start_time);
    std::cout << "Resuming sequence at " << resume_elapsed.count() << "ms"
              << std::endl;
    sequence.resume();
  });
  test_thread.detach();

  std::cout << "Starting poller..." << std::endl;
  poller.start();

  std::cout << "Test completed!" << std::endl;
  return 0;
}