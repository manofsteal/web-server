#include "websrv/network_system.hpp"
#include "websrv/log.hpp"

namespace websrv {

NetworkSystem::NetworkSystem() 
    : poller_(), 
      listenerManager_(poller_), 
      socketManager_() {
}

Listener* NetworkSystem::createListener(uint16_t port) {
    // Create listener via poller
    Listener* listener = poller_.createListener();
    if (!listener) {
        LOG_ERROR("Failed to create listener");
        return nullptr;
    }
    
    // Start listening on the port
    if (!listener->start(port)) {
        LOG_ERROR("Failed to start listener on port ", port);
        return nullptr;
    }
    
    // Automatically register with manager
    listenerManager_.addListener(listener);
    
    return listener;
}

Socket* NetworkSystem::createSocket(const std::string& host, uint16_t port) {
    // Create socket via poller
    Socket* socket = poller_.createSocket();
    if (!socket) {
        LOG_ERROR("Failed to create socket");
        return nullptr;
    }
    
    // Connect to host:port
    if (!socket->start(host, port)) {
        // Note: start() may return true for EINPROGRESS (async connect)
        // so we don't treat this as a hard error
    }
    
    // Automatically register with manager
    socketManager_.addSocket(socket);
    
    return socket;
}

std::vector<NetworkEvent> NetworkSystem::poll(int timeout_ms) {
    std::vector<NetworkEvent> events;
    
    // 1. Poll for low-level events
    auto pollerEvents = poller_.poll(timeout_ms);
    
    // 2. Process listener events (new connections)
    auto connections = listenerManager_.process(pollerEvents);
    for (const auto& conn : connections) {
        // Automatically register new sockets with socket manager
        socketManager_.addSocket(conn.new_socket);
        
        // Add ACCEPTED event
        events.push_back({NetworkEvent::ACCEPTED, conn.new_socket});
    }
    
    // 3. Process socket events (data, close, error)
    auto socketResults = socketManager_.process(pollerEvents);
    for (const auto& res : socketResults) {
        NetworkEvent::Type type;
        switch (res.type) {
            case SocketResult::DATA:
                type = NetworkEvent::SOCKET_DATA;
                break;
            case SocketResult::CLOSED:
                type = NetworkEvent::SOCKET_CLOSED;
                break;
            case SocketResult::ERROR:
                type = NetworkEvent::SOCKET_ERROR;
                break;
        }
        events.push_back({type, res.socket});
    }
    
    return events;
}

void NetworkSystem::removeClosedSockets(const std::vector<NetworkEvent>& events) {
    for (const auto& event : events) {
        if (event.type == NetworkEvent::SOCKET_CLOSED || 
            event.type == NetworkEvent::SOCKET_ERROR) {
            socketManager_.removeSocket(event.socket);
        }
    }
}

// Timer management - delegate to poller
Poller::TimerID NetworkSystem::createTimer(uint64_t delay_ms, bool repeat) {
    return poller_.createTimer(delay_ms, repeat);
}

bool NetworkSystem::isTimerExpired(Poller::TimerID id) {
    return poller_.isTimerExpired(id);
}

void NetworkSystem::resetTimer(Poller::TimerID id) {
    poller_.resetTimer(id);
}

void NetworkSystem::destroyTimer(Poller::TimerID id) {
    poller_.destroyTimer(id);
}

} // namespace websrv
