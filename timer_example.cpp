#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  Poller poller;

  // Counter to track interval timer firings
  static int intervalCounter = 0;

  // Test results storage
  struct TimerTestResult {
    uint32_t expected_ms;
    int64_t actual_ms;
    int64_t error_ms;
  };
  static std::vector<TimerTestResult> timer_results;

  // Abstract test timeout lambda

  auto test_single_timeout_lambda = [&](uint32_t timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    poller.setTimeout(timeout_ms, [&, timeout_ms, start_time]() {
      auto fire_time = std::chrono::steady_clock::now();
      auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                              fire_time - start_time)
                              .count();
      auto error = actual_delay - timeout_ms;

      timer_results.push_back({timeout_ms, actual_delay, error});
      std::cout << "Timer (" << timeout_ms
                << "ms) fired! Actual: " << actual_delay
                << "ms, Error: " << error << "ms" << std::endl;
    });
  };

  auto test_two_timeout_lambda = [&](uint32_t timeout_ms,
                                     uint32_t next_timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    poller.setTimeout(timeout_ms, [&, timeout_ms, next_timeout_ms,
                                   start_time]() {
      auto fire_time = std::chrono::steady_clock::now();
      auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                              fire_time - start_time)
                              .count();
      auto error = actual_delay - timeout_ms;

      timer_results.push_back({timeout_ms, actual_delay, error});
      std::cout << "Timer (" << timeout_ms
                << "ms) fired! Actual: " << actual_delay
                << "ms, Error: " << error << "ms" << std::endl;

      test_single_timeout_lambda(next_timeout_ms);
    });
  };

  // Test setTimeout(0, callback) - should fire immediately
  test_single_timeout_lambda(0);

  // // Test setTimeout(1, callback) - should fire after 1ms
  test_single_timeout_lambda(1);

  // // Set up timeout timer (fires once after 3 seconds)
  test_single_timeout_lambda(3000);

  test_two_timeout_lambda(1000, 1);

  // Set up timeout timer in another thread
  std::thread([&]() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    test_single_timeout_lambda(0);
  }).detach();

  std::thread([&]() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    test_single_timeout_lambda(10);
  }).detach();

  // Set up interval timer (fires every 1 second)
  static std::vector<std::chrono::steady_clock::time_point> interval_fire_times;
  auto interval_start_time = std::chrono::steady_clock::now();
  poller.setInterval(1000, [&, interval_start_time]() {
    auto fire_time = std::chrono::steady_clock::now();
    interval_fire_times.push_back(fire_time);
    intervalCounter++;
    auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                            fire_time - interval_start_time)
                            .count();
    std::cout << "Interval timer fired! Count: " << intervalCounter
              << ", Delay: " << actual_delay << "ms" << std::endl;
  });

  std::cout << "Running for 6 seconds..." << std::endl;

  // Run for 6 seconds

  std::thread run_thread([&poller]() { poller.start(); });

  std::this_thread::sleep_for(std::chrono::seconds(7));
  poller.stop();
  run_thread.join();

  std::cout << "\n=== Timer test completed ===" << std::endl;
  std::cout << "- Timeout timers fired: " << timer_results.size() << std::endl;
  std::cout << "- Interval timer fired " << intervalCounter << "times."
            << std::endl;

  // Calculate and display timing accuracy
  std::cout << "\n=== Timing Accuracy Analysis ===" << std::endl;

  // Display results from timer_results
  for (const auto &result : timer_results) {
    std::cout << "Timer (" << result.expected_ms
              << "ms): Actual = " << result.actual_ms
              << "ms, Error = " << result.error_ms << "ms" << std::endl;
  }

  // Check interval timer accuracy
  std::cout << "Interval timer (1000ms intervals):" << std::endl;
  for (size_t i = 0; i < interval_fire_times.size(); i++) {
    auto actual_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                            interval_fire_times[i] - interval_start_time)
                            .count();
    auto expected_delay = (i + 1) * 1000;
    auto error = actual_delay - expected_delay;

    std::cout << "  Interval #" << (i + 1) << ": " << actual_delay
              << "ms (Expected: " << expected_delay << "ms, Error: " << error
              << "ms)" << std::endl;
  }

  // Summary
  std::cout << "\n=== Summary ===" << std::endl;
  std::cout << "Total test duration: 6000ms" << std::endl;
  std::cout << "Expected interval fires: ~6, Actual: " << intervalCounter
            << std::endl;

  return 0;
}