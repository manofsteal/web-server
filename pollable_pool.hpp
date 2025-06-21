#pragma once

#include "pollable.hpp"
#include <map>

template <typename T> struct PollablePool {
  std::map<PollableID, T> items = {};

  T *create(PollableIDManager *idManager) {
    PollableID id = idManager->allocate();
    T &item = items[id];
    item.id = id;
    return &item;
  }

  void destroy(PollableID id) { items.erase(id); }

  T *get(PollableID id) {
    auto it = items.find(id);
    return it != items.end() ? &it->second : nullptr;
  }
};