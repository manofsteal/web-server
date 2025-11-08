#pragma once

#include <vector>

namespace websrv {

struct Any {
  std::vector<char> storage = {};

  template <typename T> T *asA() {
    if (storage.size() < sizeof(T)) {
      storage.resize(sizeof(T));
    }
    return reinterpret_cast<T *>(storage.data());
  }

  template <typename T> T *toA() {
    if (storage.size() < sizeof(T)) {
      storage.resize(sizeof(T));
    }
    return new (storage.data()) T();
  }
};

} // namespace websrv