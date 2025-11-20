#include "websrv/socket_manager.hpp"
#include <poll.h>

namespace websrv {

void SocketManager::addSocket(Socket* socket) {
    if (socket) {
        sockets_[socket->id] = socket;
    }
}

void SocketManager::removeSocket(Socket* socket) {
    if (socket) {
        sockets_.erase(socket->id);
    }
}

std::vector<SocketResult> SocketManager::process(const std::vector<PollerEvent>& events) {
    std::vector<SocketResult> results;

    for (const auto& event : events) {
        auto it = sockets_.find(event.id);
        if (it == sockets_.end()) continue;
        
        Socket* socket = it->second;
        short revents = event.revents;

        // Check for errors first
        if (socket->handleError(revents)) {
            results.push_back({SocketResult::ERROR, socket});
            continue;
        }

        // Handle read
        if (revents & POLLIN) {
            if (socket->handleRead()) {
                results.push_back({SocketResult::DATA, socket});
            } else {
                // Read returned false - could be EOF or error
                results.push_back({SocketResult::CLOSED, socket});
            }
        }

        // Handle write
        if (revents & POLLOUT) {
            socket->handleWrite();
        }
    }

    // After processing events, manage POLLOUT polling for all sockets.
    // This eliminates the need for Socket::write() to call poller->enablePollout(),
    // keeping Socket independent of Poller and following the manager pattern.
    for (auto& pair : sockets_) {
        Socket* socket = pair.second;
        PollableID id = pair.first;
        
        if (socket->write_buffer.size() > 0) {
            // Socket has pending writes - enable POLLOUT to get notified when writable
            socket->poller->enablePollout(id);
        } else {
            // Write buffer is empty - disable POLLOUT to avoid unnecessary wake-ups
            socket->poller->disablePollout(id);
        }
    }

    return results;
}

} // namespace websrv
