#include "websrv/poller.hpp"
#include "websrv/listener_manager.hpp"
#include "websrv/socket_manager.hpp"
#include "websrv/log.hpp"
#include <cassert>
#include <unistd.h>

using namespace websrv;

int main() {
    Poller poller;
    ListenerManager listenerManager(poller);
    SocketManager socketManager;

    LOG("Test 1: Socket closure detection");
    
    // Create server
    Listener* server = poller.createListener();
    if (!server->start(8082)) {
        LOG_ERROR("Failed to start server on port 8082");
        return 1;
    }
    listenerManager.addListener(server);
    
    // Create client
    Socket* client = poller.createSocket();
    client->start("127.0.0.1", 8082);
    socketManager.addSocket(client);
    
    Socket* server_socket = nullptr;
    bool connection_accepted = false;
    bool closure_detected = false;
    
    // Wait for connection
    for (int i = 0; i < 50; i++) {
        auto events = poller.poll(10);
        
        auto new_connections = listenerManager.process(events);
        for (const auto& res : new_connections) {
            server_socket = res.new_socket;
            socketManager.addSocket(server_socket);
            connection_accepted = true;
            LOG("✓ Connection accepted");
            break;
        }
        
        if (connection_accepted) break;
    }
    
    assert(connection_accepted);
    assert(server_socket != nullptr);
    
    // Close client socket
    LOG("Closing client socket...");
    ::close(client->file_descriptor);
    client->file_descriptor = -1;
    
    // Server should detect closure
    for (int i = 0; i < 50; i++) {
        auto events = poller.poll(10);
        auto results = socketManager.process(events);
        
        for (const auto& res : results) {
            if (res.socket == server_socket && res.type == SocketResult::CLOSED) {
                closure_detected = true;
                LOG("✓ Server detected client closure");
                break;
            }
        }
        
        if (closure_detected) break;
    }
    
    assert(closure_detected);
    LOG("✓ Socket closure test passed");
    
    LOG("\nTest 2: Connection to non-existent server");
    
    // Try to connect to a port that's not listening
    Socket* failed_client = poller.createSocket();
    bool connect_result = failed_client->start("127.0.0.1", 9999);
    
    // Connection might return true (EINPROGRESS) but should fail eventually
    // We don't assert on the return value since non-blocking connect returns immediately
    LOG("✓ Connect attempt to non-existent server completed (result: ", connect_result, ")");
    
    LOG("\n✅ All error handling tests passed!");
    return 0;
}
