

#pragma once

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

class Poller;

struct Sequence {
  // Constructor
  Sequence(Poller &poller);

  // Tasks
  void addTask(std::function<void()> callback, uint32_t timeout_ms = 0);
  void addDelay(uint32_t delay_ms);
  void clearTasks();

  // Control
  void start();
  void stop();

protected:
  void executeNextTask();

private:
  struct SequenceTask {
    std::function<void()> callback;
    uint32_t timeout_ms;
    bool is_delay;
  };

  std::vector<SequenceTask> tasks;
  size_t current_task_index = 0;
  bool running = false;

  // use for poll-Timer to track the current task
  Poller &poller;
  uint32_t current_timer_id = 0;
};