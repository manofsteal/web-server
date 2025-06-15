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

Timer::Timer() {
  // Initialize member variables
  isInterval = false;
  intervalMs = 0;

  // Initialize function pointers to defaults
  callback = [](Any *data) {};
  initFunction = [](Any *data) -> bool { return true; };
  startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    return false;
  };
  stopFunction = [](Any *data) {};
  handleExpirationFunction = [](Any *data) {};
  cleanupFunction = [](Any *data) {};

  // Initialize platform-specific data and functions
  setupPlatformTimer();
}

bool Timer::init(PollableType timerType) {
  type = timerType;
  id = 0;
  file_descriptor = -1;

  // Call platform-specific init function
  return initFunction(&data);
}

bool Timer::setTimeout(uint32_t milliseconds, Callback cb) {
  callback = cb;
  isInterval = false;
  intervalMs = milliseconds;

  return startFunction(&data, milliseconds);
}

bool Timer::setInterval(uint32_t milliseconds, Callback cb) {
  callback = cb;
  isInterval = true;
  intervalMs = milliseconds;

  return startFunction(&data, milliseconds);
}

void Timer::stop() { stopFunction(&data); }

void Timer::handleExpiration() {
  handleExpirationFunction(&data);

  // Call user callback
  callback(&data);

  // For intervals, restart the timer
  if (isInterval) {
    startFunction(&data, intervalMs);
  }
}

void Timer::setupPlatformTimer() {
#ifdef __linux__
  // Initialize Linux timer data
  LinuxTimerData *linux_data = data.asA<LinuxTimerData>();
  linux_data->timer_fd = -1;

  // Set up platform-specific function pointers
  initFunction = [this](Any *data) -> bool {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();

    linux_data->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (linux_data->timer_fd < 0) {
      return false;
    }

    this->file_descriptor = linux_data->timer_fd;
    this->type = PollableType::TIMER;
    return true;
  };

  startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();
    if (linux_data->timer_fd < 0) {
      return false;
    }

    struct itimerspec timer_spec;
    memset(&timer_spec, 0, sizeof(timer_spec));

    timer_spec.it_value.tv_sec = milliseconds / 1000;
    timer_spec.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_interval.tv_nsec = 0;

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

    uint64_t expirations;
    read(linux_data->timer_fd, &expirations, sizeof(expirations));
  };

  handleExpirationFunction = [](Any *data) {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();
    if (linux_data->timer_fd >= 0) {
      uint64_t expirations;
      read(linux_data->timer_fd, &expirations, sizeof(expirations));
    }
  };

  cleanupFunction = [](Any *data) {
    LinuxTimerData *linux_data = data->asA<LinuxTimerData>();
    if (linux_data->timer_fd >= 0) {
      close(linux_data->timer_fd);
      linux_data->timer_fd = -1;
    }
  };

#endif
}

void Timer::cleanup() {
  // Stop the timer before cleanup
  stop();

  // Platform-specific cleanup
  cleanupFunction(&data);

  // Reset all function pointers to defaults
  resetToDefaults();
}

void Timer::resetToDefaults() {
  callback = [](Any *data) {};
  initFunction = [](Any *data) -> bool { return true; };
  startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    return false;
  };
  stopFunction = [](Any *data) {};
  handleExpirationFunction = [](Any *data) {};
  cleanupFunction = [](Any *data) {};
  file_descriptor = -1;
}
