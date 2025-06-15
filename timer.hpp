#pragma once
#include "pollable.hpp"
#include <cstdint>
#include <functional>

#include "any.hpp"

struct Timer : Pollable {
  using Callback = std::function<void(Any *data)>;
  using InitFunction = std::function<bool(Any *data)>;
  using StartFunction = std::function<bool(Any *data, uint32_t milliseconds)>;
  using StopFunction = std::function<void(Any *data)>;
  using HandleExpirationFunction = std::function<void(Any *data)>;

  Any data;

  Callback callback = [](Any *data) {};
  InitFunction initFunction = [](Any *data) -> bool { return true; };
  StartFunction startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    return false;
  };
  StopFunction stopFunction = [](Any *data) {};
  HandleExpirationFunction handleExpirationFunction = [](Any *data) {};

  bool isInterval = false;
  uint32_t intervalMs = 0;

  // Constructor
  Timer();

  bool init(PollableType timerType = PollableType::TIMER) {
    type = timerType;
    id = 0;
    file_descriptor = -1;

    // Call platform-specific init function
    return initFunction(&data);
  }

  void cleanup();

  // Common timer APIs
  bool setTimeout(uint32_t milliseconds, Callback cb) {
    callback = cb;
    isInterval = false;
    intervalMs = milliseconds;

    return startFunction(&data, milliseconds);
  }

  bool setInterval(uint32_t milliseconds, Callback cb) {
    callback = cb;
    isInterval = true;
    intervalMs = milliseconds;

    return startFunction(&data, milliseconds);
  }

  void stop() { stopFunction(&data); }

  void handleExpiration() {
    handleExpirationFunction(&data);

    // Call user callback
    callback(&data);

    // For intervals, restart the timer
    if (isInterval) {
      startFunction(&data, intervalMs);
    }
  }
};