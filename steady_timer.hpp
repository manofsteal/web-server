#pragma once
#include "steady_clock.hpp"

class SteadyTimer {
public:
    SteadyTimer() {
        reset();
    }

    void reset() {
        start_time = SteadyClock::now();
    }

    bool isExpiredMs(int ms) const {
        return getElapsedMs() >= ms;
    }

    int getElapsedMs() const {
        return SteadyClock::elapsedMs(start_time);
    }

private:
    SteadyClock::TimePoint start_time;
};