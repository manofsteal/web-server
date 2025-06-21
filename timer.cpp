#include "timer.hpp"

// ============================================================================
// Platform-specific includes and data structures
// ============================================================================

#ifdef __linux__
#include <cstring>
#include <sys/timerfd.h>
#include <unistd.h>

struct LinuxTimerData {
  int timer_fd = -1;
};
#endif

// ============================================================================
// Timer implementation
// ============================================================================

Timer::Timer() : Pollable() {
  // Initialize member variables

  type = PollableType::TIMER;

  onEvent = [this](short revents) { eventFunction(&data); };

  // Initialize platform-specific data and functions
  setupPlatformTimer();
}

bool Timer::start() {
  // Just call startFunction - it will handle initialization and starting
  return startFunction(&data, intervalMs);
}

void Timer::stop() {
  // Stop the timer
  stopFunction(&data);
  // Reset all function pointers to defaults
  resetToDefaults();
}

bool Timer::setTimeout(uint32_t milliseconds, UserCallback cb) {
  userCallback = cb;
  intervalMs = milliseconds;
  isInterval = false;
  return start();
}

bool Timer::setInterval(uint32_t milliseconds, UserCallback cb) {
  userCallback = cb;
  intervalMs = milliseconds;
  isInterval = true;
  return start();
}

void Timer::setupPlatformTimer() {
#ifdef __linux__
  // Initialize Linux timer data
  LinuxTimerData *linux_data = data.asA<LinuxTimerData>();
  linux_data->timer_fd = -1;

  // Set up platform-specific function pointers
  startFunction = [this](Any *data, uint32_t milliseconds) -> bool {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();

    // Initialize if not already done
    if (linux_data->timer_fd == -1) {
      linux_data->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

      if (linux_data->timer_fd < 0) {
        return false;
      }

      this->file_descriptor = linux_data->timer_fd;
      this->type = PollableType::TIMER;
    }

    // Start the timer
    if (linux_data->timer_fd < 0) {
      return false;
    }

    struct itimerspec timer_spec;
    memset(&timer_spec, 0, sizeof(timer_spec));

    // Handle 0ms case - convert to 1ns since timerfd doesn't fire with 0/0
    if (milliseconds == 0) {
      timer_spec.it_value.tv_sec = 0;
      timer_spec.it_value.tv_nsec = 1; // 1 nanosecond
    } else {
      timer_spec.it_value.tv_sec = milliseconds / 1000;
      timer_spec.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
    }

    // Set interval for repeating timers
    if (this->isInterval) {
      if (milliseconds == 0) {
        timer_spec.it_interval.tv_sec = 0;
        timer_spec.it_interval.tv_nsec = 1; // 1 nanosecond
      } else {
        timer_spec.it_interval.tv_sec = milliseconds / 1000;
        timer_spec.it_interval.tv_nsec = (milliseconds % 1000) * 1000000;
      }
    } else {
      timer_spec.it_interval.tv_sec = 0;
      timer_spec.it_interval.tv_nsec = 0;
    }

    return timerfd_settime(linux_data->timer_fd, 0, &timer_spec, nullptr) == 0;
  };

  stopFunction = [](Any *data) {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();
    if (linux_data->timer_fd < 0) {
      return;
    }

    struct itimerspec timer_spec;
    memset(&timer_spec, 0, sizeof(timer_spec));
    timerfd_settime(linux_data->timer_fd, 0, &timer_spec, nullptr);

    if (linux_data->timer_fd >= 0) {
      close(linux_data->timer_fd);
      linux_data->timer_fd = -1;
    }
  };

  eventFunction = [this](Any *data) {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();
    if (linux_data->timer_fd >= 0) {
      uint64_t expirations;
      read(linux_data->timer_fd, &expirations, sizeof(expirations));
    }

    userCallback();
  };

#endif
}

void Timer::resetToDefaults() {
  userCallback = []() {};
  startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    return false;
  };
  stopFunction = [](Any *data) {};
  file_descriptor = -1;
}
