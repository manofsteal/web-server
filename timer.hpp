#pragma once
#include "pollable.hpp"
#include <cstdint>
#include <functional>

#include "any.hpp"

struct Timer : Pollable {

  using UserCallback = std::function<void()>;
  using StartFunction = std::function<bool(Any *data, uint32_t milliseconds)>;
  using StopFunction = std::function<void(Any *data)>;
  using EventFunction = std::function<void(Any *data)>;

  Any data = Any{};
  StartFunction startFunction = [](Any *data, uint32_t milliseconds) -> bool {
    return false;
  };
  StopFunction stopFunction = [](Any *data) {};
  EventFunction eventFunction = [](Any *data) {};

  UserCallback userCallback = []() {};

  bool isInterval = false;
  uint32_t intervalMs = 0;

  // Constructor
  Timer();

  bool setTimeout(uint32_t milliseconds, UserCallback cb);
  bool setInterval(uint32_t milliseconds, UserCallback cb);
  void stop();

private:
  bool start();

  void setupPlatformTimer();
  void resetToDefaults();
};