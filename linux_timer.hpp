#pragma once
#include "timer.hpp"
#include <unistd.h>
#include <cstring>
#include <sys/timerfd.h>

struct LinuxTimerData {
    int timer_fd = -1;
};

struct LinuxTimer : Timer {
    LinuxTimer() {
        // Initialize the Any storage for LinuxTimerData
        LinuxTimerData* linux_data = data.asA<LinuxTimerData>();
        linux_data->timer_fd = -1;
        
        initFunction = [this](Any* data) -> bool {
            LinuxTimerData* linux_data = data->asA<LinuxTimerData>();
            
            linux_data->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
            if (linux_data->timer_fd < 0) {
                return false;
            }
            file_descriptor = linux_data->timer_fd;
            type = PollableType::TIMER;
            return true;
        };

        startFunction = [this](Any* data, uint32_t milliseconds) -> bool {
            LinuxTimerData* linux_data = data->asA<LinuxTimerData>();
            if (linux_data->timer_fd < 0) {
                return false;
            }

            struct itimerspec timer_spec;
            memset(&timer_spec, 0, sizeof(timer_spec));
            
            timer_spec.it_value.tv_sec = milliseconds / 1000;
            timer_spec.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
            timer_spec.it_interval.tv_sec = 0;
            timer_spec.it_interval.tv_nsec = 0;

            return timerfd_settime(linux_data->timer_fd, 0, &timer_spec, nullptr) == 0;
        };

        stopFunction = [this](Any* data) {
            LinuxTimerData* linux_data = data->asA<LinuxTimerData>();
            if (linux_data->timer_fd < 0) {
                return;
            }

            struct itimerspec timer_spec;
            memset(&timer_spec, 0, sizeof(timer_spec));
            timerfd_settime(linux_data->timer_fd, 0, &timer_spec, nullptr);
            
            uint64_t expirations;
            read(linux_data->timer_fd, &expirations, sizeof(expirations));
        };

        handleExpirationFunction = [this](Any* data) {
            uint64_t expirations;
            read(file_descriptor, &expirations, sizeof(expirations));
        };
    }

    void cleanup() {
        LinuxTimerData* linux_data = data.asA<LinuxTimerData>();
        stop();
        if (linux_data->timer_fd >= 0) {
            close(linux_data->timer_fd);
            linux_data->timer_fd = -1;
        }
        Timer::cleanup();
    }
}; 