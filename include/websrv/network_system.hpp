#pragma once

#include "websrv/poller.hpp"
#include "websrv/listener_manager.hpp"
#include "websrv/socket_manager.hpp"
#include <vector>

namespace websrv {

// Unified network event type
struct NetworkEvent {
    enum Type { 
        ACCEPTED,       // New connection accepted
        SOCKET_DATA,    // Data received on socket
        SOCKET_CLOSED,  // Socket connection closed
        SOCKET_ERROR    // Socket error occurred
    };
    
    Type type;
    Socket* socket;
};

// Simplified network API - wraps Poller, ListenerManager, and SocketManager
class NetworkSystem {
public:
    NetworkSystem();
    
    // Factory methods - automatically register with appropriate managers
    // Returns nullptr on failure
    Listener* createListener(uint16_t port);
    Socket* createSocket(const std::string& host, uint16_t port);
    
    // Main loop method - polls and processes all events
    // Returns unified NetworkEvent list
    std::vector<NetworkEvent> poll(int timeout_ms);
    
    // Remove closed/errored sockets from manager
    void removeClosedSockets(const std::vector<NetworkEvent>& events);
    
    // Timer management (delegates to internal poller)
    Poller::TimerID createTimer(uint64_t delay_ms, bool repeat);
    bool isTimerExpired(Poller::TimerID id);
    void resetTimer(Poller::TimerID id);
    void destroyTimer(Poller::TimerID id);
    
    // Advanced: Access to underlying components if needed
    Poller& getPoller() { return poller_; }
    ListenerManager& getListenerManager() { return listenerManager_; }
    SocketManager& getSocketManager() { return socketManager_; }

private:
    Poller poller_;
    ListenerManager listenerManager_;
    SocketManager socketManager_;
};

} // namespace websrv
