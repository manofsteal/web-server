#pragma once
#include <chrono>

struct SteadyClock {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    static TimePoint now() {
        return Clock::now();
    }

    static TimePoint addMilliseconds(const TimePoint& tp, int ms) {
        return tp + std::chrono::milliseconds(ms);
    }

    static int elapsedMs(const TimePoint& since) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(now() - since).count();
    }
}; 