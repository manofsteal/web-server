#pragma once
#include "pollable.hpp"
#include <cstdint>
#include <functional>

#include "any.hpp"

struct Timer : Pollable {
  using Callback = std::function<void(Any *data)>;
  using StartFunction = std::function<bool(Any *data, uint32_t milliseconds)>;
  using StopFunction = std::function<void(Any *data)>;
  using HandleExpirationFunction = std::function<void(Any *data)>;
  using CleanupFunction = std::function<void(Any *data)>;

  Any data;

  Callback callback;
  StartFunction startFunction;
  StopFunction stopFunction;
  HandleExpirationFunction handleExpirationFunction;
  CleanupFunction cleanupFunction;

  bool isInterval;
  uint32_t intervalMs;

  // Constructor
  Timer();

  bool setTimeout(uint32_t milliseconds, Callback cb);
  bool setInterval(uint32_t milliseconds, Callback cb);
  void stop();

  // Internal methods (called by poller)
  void handleExpiration();

private:
  bool start();

  void setupPlatformTimer();
  void resetToDefaults();
};