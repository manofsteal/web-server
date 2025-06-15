#pragma once
#include <poll.h>
#include <errno.h>
#include <stdexcept>
#include <vector>
#include <signal.h>  // For sigaction and siginfo_t
#include "pollable.hpp"
#include "socket.hpp"
#include "listener.hpp"
#include "timer.hpp"
#include "pollable_pool_manager.hpp"
#include <map>
#include <functional>

struct Poller {
    struct PollEntry {
        Pollable* pollable;
        short events;  // poll events (POLLIN, POLLOUT, etc.)
        std::function<void()> callback;
        
        bool init(Pollable* p, short ev, std::function<void()> cb) {
            pollable = p;
            events = ev;
            callback = cb;
            return true;
        }
        
        void cleanup() {
            pollable = nullptr;
            callback = nullptr;
            events = 0;
        }
    };

    PollablePoolManager poolManager;
    std::vector<pollfd> pollFds;
    std::map<PollableID, PollEntry> pollEntries;
    bool running = false;

    
    // Factory methods
    Socket* createSocket();
    Timer* createTimer();
    Listener* createListener();

    void remove(PollableID id);
    void run();
    void stop();

    void cleanup();

protected:

    void addSocket(Socket* socket);
    void addTimer(Timer* timer);
    void addListener(Listener* listener);


};

