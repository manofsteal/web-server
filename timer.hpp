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
  using CleanupFunction = std::function<void(Any *data)>;

  Any data;

  Callback callback;
  InitFunction initFunction;
  StartFunction startFunction;
  StopFunction stopFunction;
  HandleExpirationFunction handleExpirationFunction;
  CleanupFunction cleanupFunction;

  bool isInterval;
  uint32_t intervalMs;

  // Constructor
  Timer();

  // Public methods
  bool init(PollableType timerType = PollableType::TIMER);
  void cleanup();
  bool setTimeout(uint32_t milliseconds, Callback cb);
  bool setInterval(uint32_t milliseconds, Callback cb);
  void stop();
  void handleExpiration();

private:
  void setupPlatformTimer();
  void resetToDefaults();
};