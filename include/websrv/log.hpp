#pragma once
#include <iostream>
#include <sstream>

// Simple logging macros
#define LOG(...)                                                               \
  do {                                                                         \
    std::ostringstream oss;                                                    \
    log_impl(oss, __VA_ARGS__);                                                \
    std::cout << oss.str() << std::endl;                                       \
  } while (0)

#define LOG_ERROR(...)                                                         \
  do {                                                                         \
    std::ostringstream oss;                                                    \
    oss << "[ERROR] ";                                                         \
    log_impl(oss, __VA_ARGS__);                                                \
    std::cerr << oss.str() << std::endl;                                       \
  } while (0)

// Helper function to handle variadic arguments
template <typename T> void log_impl(std::ostringstream &oss, T &&arg) {
  oss << arg;
}

template <typename T, typename... Args>
void log_impl(std::ostringstream &oss, T &&first, Args &&... args) {
  oss << first;
  log_impl(oss, args...);
}
