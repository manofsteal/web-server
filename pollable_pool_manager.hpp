#pragma once
#include <cstdint>
#include <map>
#include "pollable.hpp"
#include "pollable_pool.hpp"
#include "timer.hpp"
#include "socket.hpp"
#include "listener.hpp"



struct PollablePoolManager {

    bool init() {
        return true;
    }

    void cleanup() {
        timers.items.clear();
        sockets.items.clear();
        listeners.items.clear();
    }

    PollableIDManager id_manager;

    PollablePool<Timer> timers;
    PollablePool<Socket> sockets;
    PollablePool<Listener> listeners;
}; 