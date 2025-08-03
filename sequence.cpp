#include "sequence.hpp"
#include "poller.hpp"

Sequence::Sequence(Poller &poller) : poller(poller) {}

void Sequence::addTask(Poller::TimerCallback callback, uint32_t timeout_ms) {
  tasks.push_back({callback, timeout_ms, false});
}

void Sequence::addDelay(uint32_t delay_ms) {
  tasks.push_back({nullptr, delay_ms, true});
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
  // Use remaining time from pause if available, otherwise use task's original timeout
  uint32_t timeout_to_use = remaining_time_ms > 0 ? remaining_time_ms : task.timeout_ms;
  
  // Record when this task started for pause/resume timing calculations
  task_start_time = std::chrono::steady_clock::now();
  remaining_time_ms = 0; // Reset remaining time since we're using it now

  // Set up a timer for this task
  current_timer_id = poller.setTimeout(timeout_to_use, [this, task]() {
    if (!running || paused) {
      return;
    }

    // Execute the callback if it's not a delay
    if (!task.is_delay && task.callback) {
      task.callback();
    }

    // Move to next task
    current_task_index++;
    current_timer_id = 0;

    // Execute next task
    executeNextTask();
  });
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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - task_start_time).count();
    
    if (current_task_index < tasks.size()) {
      const auto &task = tasks[current_task_index];
      uint32_t total_time = remaining_time_ms > 0 ? remaining_time_ms : task.timeout_ms;
      
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