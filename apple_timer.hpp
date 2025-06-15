#pragma once
#include "timer.hpp"
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <memory>

struct AppleTimerData {
    int kqueue_fd = -1;
    int timer_id = 1;
    bool timer_active = false;
};

// Helper function to create and configure an Apple timer
inline Timer* createAppleTimer() {
    Timer* timer = new Timer();
    auto data = std::make_shared<AppleTimerData>();
    
    // Initialize kqueue
    data->kqueue_fd = kqueue();
    if (data->kqueue_fd == -1) {
        delete timer;
        return nullptr;
    }
    
    timer->file_descriptor = data->kqueue_fd;
    
    // Set platform-specific functions directly
    timer->startFunction = [data](uint32_t milliseconds) -> bool {
        if (data->kqueue_fd < 0) return false;
        
        struct kevent kev;
        EV_SET(&kev, data->timer_id, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, milliseconds, nullptr);
        
        if (kevent(data->kqueue_fd, &kev, 1, nullptr, 0, nullptr) == -1) {
            return false;
        }
        
        data->timer_active = true;
        return true;
    };
    
    timer->stopFunction = [data]() {
        if (data->kqueue_fd < 0 || !data->timer_active) return;
        
        struct kevent kev;
        EV_SET(&kev, data->timer_id, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
        
        kevent(data->kqueue_fd, &kev, 1, nullptr, 0, nullptr);
        data->timer_active = false;
    };
    
    timer->handleExpirationFunction = [data]() {
        if (data->kqueue_fd < 0) return;
        
        struct kevent events[1];
        struct timespec timeout = {0, 0}; // Non-blocking
        
        int n = kevent(data->kqueue_fd, nullptr, 0, events, 1, &timeout);
        if (n > 0 && events[0].filter == EVFILT_TIMER) {
            // Timer expired - handled by base Timer::handleExpiration
        }
    };
    
    // Custom cleanup to close kqueue
    auto originalStopFunction = timer->stopFunction;
    timer->stopFunction = [data, originalStopFunction]() {
        originalStopFunction();
        if (data->kqueue_fd >= 0) {
            close(data->kqueue_fd);
            data->kqueue_fd = -1;
        }
    };
    
    return timer;
} 