#include "sequence.hpp"
#include "poller.hpp"

Sequence::Sequence(Poller &poller) : poller(poller) {}

void Sequence::addTask(Poller::TimerCallback callback) {
  tasks.push_back({callback, 0, false});
}

void Sequence::addWait(uint32_t period_ms) {
  tasks.push_back({[] {}, period_ms, false});
}

void Sequence::addWait(std::function<bool()> condition, uint32_t period_ms,
                       uint32_t timeout_ms) {

  const auto taskIndex = tasks.size();
  tasks.push_back({[] {}, period_ms, true, condition, timeout_ms});
}

void Sequence::clearTasks() {
  tasks.clear();
  current_task_index = 0;

  // Clear any running timer
  if (current_timer_id != 0) {
    poller.clearTimeout(current_timer_id);
    current_timer_id = 0;
  }
}

void Sequence::start() {
  if (running) {
    return;
  }

  running = true;
  current_task_index = 0;

  // Start executing the first task
  executeNextTask();
}

void Sequence::stop() {
  running = false;
  paused = false;
  remaining_time_ms = 0;

  // Clear any running timer
  if (current_timer_id != 0) {
    poller.clearTimeout(current_timer_id);
    current_timer_id = 0;
  }
}

void Sequence::postNextTask() {
  // Move to next task
  current_task_index++;

  // reset timer id
  current_timer_id = 0;

  // Execute next task
  executeNextTask();
}

void Sequence::executeNextTask() {
  if (!running || current_task_index >= tasks.size()) {
    // Sequence is finished or stopped
    running = false;
    std::cout << "Sequence finished" << std::endl;
    return;
  }

  if (paused) {
    return;
  }

  const auto &task = tasks[current_task_index];
  // Use remaining time from pause if available, otherwise use task's original
  // period
  uint32_t period_to_use =
      remaining_time_ms > 0 ? remaining_time_ms : task.period_ms;

  // Record when this task started for pause/resume timing calculations
  task_start_time = std::chrono::steady_clock::now();
  remaining_time_ms = 0; // Reset remaining time since we're using it now

  // Set up a timer for this task
  current_timer_id = poller.setTimeout(period_to_use, [this, task]() {
    if (!running || paused) {
      return;
    }

    if (task.is_condition) {

      executeCondition();

    } else {

      task.callback();

      postNextTask();
    }
  });
}

void Sequence::executeCondition() {
  if (!running) {
    std::cout << "Sequence finished" << std::endl;
    return;
  }

  if (paused) {
    std::cout << "Sequence paused" << std::endl;
    return;
  }

  auto &task = tasks[current_task_index];

  if (!task.is_condition) {
    std::cerr << "Task not a wait-condition " << std::endl;
    return;
  }

  std::cerr << "Check condition" << std::endl;

  if (task.condition()) {

    postNextTask();

  } else {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - task_start_time)
                       .count();

    if (elapsed >= task.timeout_ms) {

      postNextTask();

    } else {

      std::cerr << "Check again" << std::endl;

      current_timer_id =
          poller.setTimeout(task.period_ms, [this] { executeCondition(); });
    }
  }
}
// Pause the sequence execution
// This function stops the current timer and calculates how much time remains
// for the current task, so it can be resumed later from the correct position
void Sequence::pause() {
  if (!running || paused) {
    return;
  }

  paused = true;

  // Calculate remaining time for current task
  if (current_timer_id != 0) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - task_start_time)
                       .count();

    if (current_task_index < tasks.size()) {
      const auto &task = tasks[current_task_index];
      uint32_t total_time =
          remaining_time_ms > 0 ? remaining_time_ms : task.timeout_ms;

      // Store remaining time if task hasn't completed yet
      if (elapsed < total_time) {
        remaining_time_ms = total_time - elapsed;
      } else {
        remaining_time_ms = 0;
      }
    }

    // Clear the current timer to stop execution
    poller.clearTimeout(current_timer_id);
    current_timer_id = 0;
  }
}

// Resume the sequence execution from where it was paused
// This function clears the paused state and continues executing tasks,
// using any remaining time that was calculated during pause
void Sequence::resume() {
  if (!running || !paused) {
    return;
  }

  paused = false;

  // Continue with the current task using remaining time if any
  executeNextTask();
}