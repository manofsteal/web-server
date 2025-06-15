#pragma once
#include <cstdint>

using PollableID = uint32_t;

enum class PollableType { SOCKET, LISTENER, TIMER };

struct Pollable {
  PollableType type;
  PollableID id;
  int file_descriptor;

  // Constructor to replace init()
  explicit Pollable(PollableType t) : type(t), id(0), file_descriptor(-1) {}

  // Default constructor for cases where type is set later
  Pollable() : type(PollableType::SOCKET), id(0), file_descriptor(-1) {}
};

class PollableIDManager {
public:
  uint32_t next_id = 0;
  PollableID allocate() { return next_id++; }
};