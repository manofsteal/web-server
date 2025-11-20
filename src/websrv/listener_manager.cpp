#include "websrv/listener_manager.hpp"
#include "websrv/log.hpp"
#include <poll.h>

namespace websrv {

ListenerManager::ListenerManager(Poller& poller) : poller_(poller) {}

void ListenerManager::addListener(Listener* listener) {
    if (listener) {
        listeners_[listener->id] = listener;
    }
}

void ListenerManager::removeListener(Listener* listener) {
    if (listener) {
        listeners_.erase(listener->id);
    }
}

std::vector<ConnectionResult> ListenerManager::process(const std::vector<PollerEvent>& events) {
    std::vector<ConnectionResult> results;

    for (const auto& event : events) {
        auto it = listeners_.find(event.id);
        if (it != listeners_.end() && (event.revents & POLLIN)) {
            Listener* listener = it->second;
            Socket* new_socket = listener->handleAccept(poller_);
            if (new_socket) {
                results.push_back({new_socket});
            }
        }
    }

    return results;
}

} // namespace websrv
