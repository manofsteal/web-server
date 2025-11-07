

#pragma once

#include "websrv/poller.hpp"
#include "websrv/steady_clock.hpp"
#include "websrv/steady_timer.hpp"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>

struct Sequence {
  // Constructor
  Sequence(Poller &poller);

  // Tasks
  void addTask(Poller::TimerCallback callback);
  void addWait(uint32_t period_ms);
  void addWait(std::function<bool()> condition, uint32_t period_ms = 10,
               uint32_t timeout_ms = 1000);
  void clearTasks();

  // Control
  void start();
  void stop();

  // Advance

  void pause();
  void resume();

protected:
  void executeNextTask();
  void postNextTask();

  void executeCondition();

private:
  using TimePoint = SteadyClock::TimePoint;
  struct SequenceTask {
    std::function<void()> callback = [] {};
    size_t period_ms;
    bool is_condition = false;

    // use for wait condition
    std::function<bool()> condition;
    uint32_t timeout_ms;
  };

  std::vector<SequenceTask> tasks;
  size_t current_task_index = 0;
  bool running = false;
  bool paused = false;

  // use for poll-Timer to track the current task
  Poller &poller;
  Poller::TimerID current_timer_id = 0;

  // For pause/resume functionality
  SteadyTimer task_timer;
  uint32_t remaining_time_ms = 0;
};