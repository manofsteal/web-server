#include "sequence.hpp"
#include "poller.hpp"

Sequence::Sequence(Poller &poller) : poller(poller) {}

void Sequence::addTask(std::function<void()> callback, uint32_t timeout_ms) {
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

  const auto &task = tasks[current_task_index];

  // Set up a timer for this task
  current_timer_id = poller.setTimeout(task.timeout_ms, [this, task]() {
    if (!running) {
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