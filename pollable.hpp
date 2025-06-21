#pragma once
#include <cstdint>
#include <functional>

using PollableID = uint32_t;

enum class PollableType { SOCKET, LISTENER, TIMER };

class Poller;
struct Pollable {
  PollableType type = PollableType::SOCKET;
  PollableID id = 0;
  int file_descriptor = -1;

  Poller *poller = nullptr;

  using EventCallback = std::function<void(short revents)>;
  using StopFunction = std::function<void()>;

  EventCallback onEvent = [](short revents) {};
  StopFunction stopFunction = []() {};
};

class PollableIDManager {
public:
  uint32_t next_id = 0;
  PollableID allocate() { return next_id++; }
};