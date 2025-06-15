#pragma once
#include "timer.hpp"
#include <unistd.h>
#include <cstring>
#include <time.h>
#include <signal.h>
#include <sys/neutrino.h>

struct QnxTimerData {
    timer_t timer_id;
    int chid = -1;  // channel ID
    int coid = -1;  // connection ID
    bool timer_created = false;
};

struct QnxTimer : Timer {
    QnxTimer() {
        // Initialize the Any storage for QnxTimerData
        QnxTimerData* qnx_data = data.asA<QnxTimerData>();
        qnx_data->chid = -1;
        qnx_data->coid = -1;
        qnx_data->timer_created = false;
        
        initFunction = [this](Any* data) -> bool {
            QnxTimerData* qnx_data = data->asA<QnxTimerData>();
            
            // Create a channel for timer notifications
            qnx_data->chid = ChannelCreate(0);
            if (qnx_data->chid == -1) {
                return false;
            }
            
            // Connect to the channel
            qnx_data->coid = ConnectAttach(ND_LOCAL_NODE, 0, qnx_data->chid, _NTO_SIDE_CHANNEL, 0);
            if (qnx_data->coid == -1) {
                ChannelDestroy(qnx_data->chid);
                return false;
            }
            
            // Create timer
            struct sigevent event;
            SIGEV_PULSE_INIT(&event, qnx_data->coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);
            
            if (timer_create(CLOCK_MONOTONIC, &event, &qnx_data->timer_id) == -1) {
                ConnectDetach(qnx_data->coid);
                ChannelDestroy(qnx_data->chid);
                return false;
            }
            
            qnx_data->timer_created = true;
            file_descriptor = qnx_data->chid;
            type = PollableType::TIMER;
            return true;
        };

        startFunction = [this](Any* data, uint32_t milliseconds) -> bool {
            QnxTimerData* qnx_data = data->asA<QnxTimerData>();
            if (!qnx_data->timer_created) {
                return false;
            }

            struct itimerspec timer_spec;
            memset(&timer_spec, 0, sizeof(timer_spec));
            
            timer_spec.it_value.tv_sec = milliseconds / 1000;
            timer_spec.it_value.tv_nsec = (milliseconds % 1000) * 1000000;
            timer_spec.it_interval.tv_sec = 0;
            timer_spec.it_interval.tv_nsec = 0;

            return timer_settime(qnx_data->timer_id, 0, &timer_spec, nullptr) == 0;
        };

        stopFunction = [this](Any* data) {
            QnxTimerData* qnx_data = data->asA<QnxTimerData>();
            if (!qnx_data->timer_created) {
                return;
            }

            struct itimerspec timer_spec;
            memset(&timer_spec, 0, sizeof(timer_spec));
            timer_settime(qnx_data->timer_id, 0, &timer_spec, nullptr);
            
            // Clear any pending pulses
            struct _pulse pulse;
            MsgReceive(qnx_data->chid, &pulse, sizeof(pulse), nullptr);
        };

        handleExpirationFunction = [this](Any* data) {
            QnxTimerData* qnx_data = data->asA<QnxTimerData>();
            
            // Receive the pulse from the timer
            struct _pulse pulse;
            MsgReceive(qnx_data->chid, &pulse, sizeof(pulse), nullptr);
        };
    }

    void cleanup() {
        QnxTimerData* qnx_data = data.asA<QnxTimerData>();
        stop();
        
        if (qnx_data->timer_created) {
            timer_delete(qnx_data->timer_id);
            qnx_data->timer_created = false;
        }
        
        if (qnx_data->coid != -1) {
            ConnectDetach(qnx_data->coid);
            qnx_data->coid = -1;
        }
        
        if (qnx_data->chid != -1) {
            ChannelDestroy(qnx_data->chid);
            qnx_data->chid = -1;
        }
        
        Timer::cleanup();
    }
};