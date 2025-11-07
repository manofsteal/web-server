#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Task represents a unit of work to be executed by the executor
using Task = std::function<void()>;

// Executor manages a thread pool for safely executing callbacks
class Executor {
private:
  std::vector<std::thread> workers;
  std::queue<Task> taskQueue;
  mutable std::mutex queueMutex; // Make mutable for const methods
  std::condition_variable condition;
  std::atomic<bool> running{false};
  size_t numThreads;

public:
  explicit Executor(size_t threadCount = std::thread::hardware_concurrency())
      : numThreads(threadCount) {}

  ~Executor() { stop(); }

  // Start the executor with worker threads
  bool start() {
    if (running.load()) {
      return false; // Already running
    }

    running.store(true);

    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] { workerLoop(); });
    }

    return true;
  }

  // Stop the executor and join all threads
  void stop() {
    if (!running.load()) {
      return; // Already stopped
    }

    running.store(false);
    condition.notify_all();

    for (auto &worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }

    workers.clear();

    // Clear remaining tasks
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!taskQueue.empty()) {
      taskQueue.pop();
    }
  }

  // Submit a task for execution
  void submit(Task task) {
    if (!running.load()) {
      return; // Executor not running
    }

    {
      std::lock_guard<std::mutex> lock(queueMutex);
      taskQueue.push(std::move(task));
    }
    condition.notify_one();
  }

  // Get the number of pending tasks
  size_t getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
  }

  // Check if executor is running
  bool isRunning() const { return running.load(); }

private:
  // Worker thread main loop
  void workerLoop() {
    while (running.load()) {
      Task task;

      {
        std::unique_lock<std::mutex> lock(queueMutex);
        condition.wait(
            lock, [this] { return !taskQueue.empty() || !running.load(); });

        if (!running.load()) {
          break;
        }

        if (!taskQueue.empty()) {
          task = std::move(taskQueue.front());
          taskQueue.pop();
        } else {
          continue;
        }
      }

      // Execute the task outside the lock
      try {
        if (task) {
          task();
        }
      } catch (...) {
        // Swallow exceptions to prevent worker thread from crashing
        // In production, you might want to log this
      }
    }
  }
};