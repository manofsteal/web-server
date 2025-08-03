

#pragma once

#include "poller.hpp"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

struct Sequence {
  // Constructor
  Sequence(Poller &poller);

  // Tasks
  void addTask(Poller::TimerCallback callback, uint32_t timeout_ms = 0);
  void addDelay(uint32_t delay_ms);
  void clearTasks();

  // Control
  void start();
  void stop();

  // Advance

  void pause();
  void resume();

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
  bool paused = false;

  // use for poll-Timer to track the current task
  Poller &poller;
  Poller::TimerID current_timer_id = 0;
  
  // For pause/resume functionality
  std::chrono::steady_clock::time_point task_start_time;
  uint32_t remaining_time_ms = 0;
};