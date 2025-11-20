#include "websrv/poller.hpp"
#include "websrv/listener_manager.hpp"
#include "websrv/socket_manager.hpp"
#include "websrv/log.hpp"
#include <cassert>

using namespace websrv;

int main() {
    Poller poller;
    ListenerManager listenerManager(poller);
    SocketManager socketManager;

    LOG("Test 1: Add and remove listener");
    
    Listener* listener1 = poller.createListener();
    listener1->start(8083);
    
    // Add listener
    listenerManager.addListener(listener1);
    LOG("✓ Listener added to manager");
    
    // Create client to trigger event
    Socket* client1 = poller.createSocket();
    client1->start("127.0.0.1", 8083);
    socketManager.addSocket(client1);
    
    // Should receive connection
    bool connection_received = false;
    for (int i = 0; i < 50; i++) {
        auto events = poller.poll(10);
        auto connections = listenerManager.process(events);
        
        if (!connections.empty()) {
            connection_received = true;
            socketManager.addSocket(connections[0].new_socket);
            LOG("✓ Connection received while listener registered");
            break;
        }
    }
    assert(connection_received);
    
    // Remove listener
    listenerManager.removeListener(listener1);
    LOG("✓ Listener removed from manager");
    
    // Create another client
    Socket* client2 = poller.createSocket();
    client2->start("127.0.0.1", 8083);
    
    // Should NOT receive connection (listener removed)
    bool unexpected_connection = false;
    for (int i = 0; i < 20; i++) {
        auto events = poller.poll(10);
        auto connections = listenerManager.process(events);
        
        if (!connections.empty()) {
            unexpected_connection = true;
            break;
        }
    }
    assert(!unexpected_connection);
    LOG("✓ No connection received after listener removed");
    
    LOG("\nTest 2: Add and remove socket");
    
    Listener* listener2 = poller.createListener();
    listener2->start(8084);
    listenerManager.addListener(listener2);
    
    Socket* client3 = poller.createSocket();
    client3->start("127.0.0.1", 8084);
    
    Socket* server_socket = nullptr;
    
    // Accept connection
    for (int i = 0; i < 50; i++) {
        auto events = poller.poll(10);
        auto connections = listenerManager.process(events);
        
        if (!connections.empty()) {
            server_socket = connections[0].new_socket;
            socketManager.addSocket(server_socket);
            LOG("✓ Server socket added to manager");
            break;
        }
    }
    assert(server_socket != nullptr);
    
    // Add client to manager
    socketManager.addSocket(client3);
    
    // Send data from server
    server_socket->write("Hello");
    
    // Client should receive data
    bool data_received = false;
    for (int i = 0; i < 50; i++) {
        auto events = poller.poll(10);
        auto results = socketManager.process(events);
        
        for (const auto& res : results) {
            if (res.socket == client3 && res.type == SocketResult::DATA) {
                data_received = true;
                LOG("✓ Client received data while registered");
                break;
            }
        }
        if (data_received) break;
    }
    assert(data_received);
    
    // Remove client from manager
    socketManager.removeSocket(client3);
    LOG("✓ Client socket removed from manager");
    
    // Send more data from server
    server_socket->write("World");
    
    // Client should NOT receive event (removed from manager)
    bool unexpected_data = false;
    for (int i = 0; i < 20; i++) {
        auto events = poller.poll(10);
        auto results = socketManager.process(events);
        
        for (const auto& res : results) {
            if (res.socket == client3) {
                unexpected_data = true;
                break;
            }
        }
        if (unexpected_data) break;
    }
    assert(!unexpected_data);
    LOG("✓ No events received after socket removed");
    
    LOG("\n✅ All manager operations tests passed!");
    return 0;
}
