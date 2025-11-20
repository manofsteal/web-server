#pragma once

#include "websrv/listener.hpp"
#include "websrv/socket.hpp"
#include "websrv/poller.hpp"
#include <vector>
#include <unordered_map>

namespace websrv {

struct ConnectionResult {
    Socket* new_socket;
};

class ListenerManager {
public:
    ListenerManager(Poller& poller);
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
    // Process events for managed listeners
    std::vector<ConnectionResult> process(const std::vector<PollerEvent>& events);

private:
    Poller& poller_;
    std::unordered_map<PollableID, Listener*> listeners_;
};

} // namespace websrv
