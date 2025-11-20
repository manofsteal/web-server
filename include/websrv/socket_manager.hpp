#pragma once

#include "websrv/socket.hpp"
#include "websrv/poller.hpp"
#include <vector>
#include <unordered_map>

namespace websrv {

struct SocketResult {
    enum Type { DATA, CLOSED, ERROR };
    Type type;
    Socket* socket;
};

class SocketManager {
public:
    void addSocket(Socket* socket);
    void removeSocket(Socket* socket);
    
    // Process events for managed sockets
    std::vector<SocketResult> process(const std::vector<PollerEvent>& events);

private:
    std::unordered_map<PollableID, Socket*> sockets_;
};

} // namespace websrv
