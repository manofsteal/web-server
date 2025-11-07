#include "websrv/poller.hpp"
#include "websrv/sequence.hpp"
#include <chrono>
#include <iostream>

int main() {
  Poller poller;
  Sequence sequence(poller);

  // Record start time
  auto start_time = std::chrono::steady_clock::now();
  auto task1_time = start_time;
  auto task2_time = start_time;
  auto task3_time = start_time;

  // Add some tasks to the sequence with timing
  sequence.addTask([&sequence, &task1_time, start_time, &poller]() {
    task1_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        task1_time - start_time);
    std::cout << "Task 1: Hello from sequence! (executed at " << elapsed.count()
              << "ms)" << std::endl;

    sequence.addTask([&poller, &sequence] {
      std::cout << "Task 4: queue from Task 1, Stop poller" << std::endl;

      // // !!!!! Stop poller to end the program
      // poller.stop();
    });
  }); // Execute immediately

  sequence.addWait(500); // Wait 500ms

  sequence.addTask([&sequence, &task2_time, &task1_time]() {
    task2_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        task2_time - task1_time);
    std::cout << "Task 2: Second task executed! (delay from task 1: "
              << elapsed.count() << "ms)" << std::endl;

    //  !!! Un-comment code below, the sequence will be stopped after Task 2
    // std::cout << "Cancel sequence" << std::endl;
    // sequence.stop();
  }); // Execute immediately

  sequence.addWait(2000); // Wait 1 second

  sequence.addTask([&sequence, &task2_time, &task1_time]() {
    std::cout << "Done Wait(1000)" << std::endl;
  }); // Execute immediately

  // Test condition-based addWait - wait for a condition that becomes true after
  // 2 seconds
  static bool condition_met = false;
  static auto condition_start_time = std::chrono::steady_clock::now();

  sequence.addWait(
      [] {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - condition_start_time)
                           .count();
        if (elapsed >= 5000) {
          condition_met = true;
          std::cout << "Condition Wait: Condition became true after " << elapsed
                    << "ms" << std::endl;
          return true;
        } else {
          std::cout << "Condition Elapsed: " << elapsed << "ms" << std::endl;
        }
        return false;
      },
      100,  // Check every 100ms
      10000 // Timeout after 5 seconds
  );

  // Test condition-based addWait with timeout - wait for a condition that never
  // becomes true
  sequence.addWait(
      [] {
        std::cout << "Timeout Wait: Checking condition (will timeout)..."
                  << std::endl;
        return false; // Never becomes true
      },
      200, // Check every 200ms
      1000 // Timeout after 1 second
  );

  sequence.addTask([&poller, &task3_time, &task2_time, start_time]() {
    task3_time = std::chrono::steady_clock::now();
    auto elapsed_from_task2 =
        std::chrono::duration_cast<std::chrono::milliseconds>(task3_time -
                                                              task2_time);
    auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        task3_time - start_time);
    std::cout << "Task 3: (delay from task 2: " << elapsed_from_task2.count()
              << "ms, total time: " << total_elapsed.count() << "ms)"
              << std::endl;
  });

  // Start the sequence
  sequence.start();

  // std::thread([&]() {
  //   std::this_thread::sleep_for(std::chrono::seconds(7));

  //   std::cout << "Start again, only work if Task 4 do not stop poller"
  //             << std::endl;
  //   sequence.start();
  // }).detach();

  // Start the poller (this will block until stopped)
  std::cout << "Starting poller at time 0ms..." << std::endl;
  auto poller_start = std::chrono::steady_clock::now();

  poller.start();

  std::cout << "Poller stopped" << std::endl;
  auto poller_end = std::chrono::steady_clock::now();

  auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      poller_end - poller_start);
  std::cout << "Sequence completed! Total execution time: "
            << total_duration.count() << "ms" << std::endl;

  return 0;
}