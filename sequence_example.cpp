#include "poller.hpp"
#include "sequence.hpp"
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
  sequence.addTask(
      [&sequence, &task1_time, start_time, &poller]() {
        task1_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            task1_time - start_time);
        std::cout << "Task 1: Hello from sequence! (executed at "
                  << elapsed.count() << "ms)" << std::endl;

        sequence.addTask(
            [&poller, &sequence] {
              std::cout << "Task 4: queue from Task 1, Stop poller"
                        << std::endl;

              // // !!!!! Stop poller to end the program
              // poller.stop();
            },
            2000);
      },
      1000); // Execute after 1 second

  sequence.addDelay(500); // Wait 500ms

  sequence.addTask(
      [&sequence, &task2_time, &task1_time]() {
        task2_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            task2_time - task1_time);
        std::cout << "Task 2: Second task executed! (delay from task 1: "
                  << elapsed.count() << "ms)" << std::endl;

        //  !!! Un-comment code below, the sequence will be stopped after Task 2
        // std::cout << "Cancel sequence" << std::endl;
        // sequence.stop();
      },
      0); // Execute immediately

  sequence.addDelay(1000); // Wait 1 second

  sequence.addTask(
      [&poller, &task3_time, &task2_time, start_time]() {
        task3_time = std::chrono::steady_clock::now();
        auto elapsed_from_task2 =
            std::chrono::duration_cast<std::chrono::milliseconds>(task3_time -
                                                                  task2_time);
        auto total_elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(task3_time -
                                                                  start_time);
        std::cout << "Task 3: (delay from task 2: "
                  << elapsed_from_task2.count()
                  << "ms, total time: " << total_elapsed.count() << "ms)"
                  << std::endl;
      },
      0);

  // Start the sequence
  sequence.start();

  std::thread([&]() {
    std::this_thread::sleep_for(std::chrono::seconds(7));

    std::cout << "Start again, only work if Task 4 do not stop poller"
              << std::endl;
    sequence.start();
  }).detach();

  // Start the poller (this will block until stopped)
  std::cout << "Starting poller at time 0ms..." << std::endl;
  auto poller_start = std::chrono::steady_clock::now();

  poller.start();
  auto poller_end = std::chrono::steady_clock::now();

  auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      poller_end - poller_start);
  std::cout << "Sequence completed! Total execution time: "
            << total_duration.count() << "ms" << std::endl;

  return 0;
}