#include "websrv/poller.hpp"
#include "websrv/listener_manager.hpp"
#include "websrv/socket_manager.hpp"
#include "websrv/log.hpp"
#include <cassert>
#include <map>

using namespace websrv;

int main() {
    Poller poller;
    ListenerManager listenerManager(poller);
    SocketManager socketManager;

    // Create server
    Listener* server = poller.createListener();
    if (!server->start(8081)) {
        LOG_ERROR("Failed to start server on port 8081");
        return 1;
    }
    listenerManager.addListener(server);
    LOG("Server started on port 8081");

    // Create 3 clients
    const int NUM_CLIENTS = 3;
    Socket* clients[NUM_CLIENTS];
    std::map<Socket*, int> client_ids;
    
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients[i] = poller.createSocket();
        clients[i]->start("127.0.0.1", 8081);
        socketManager.addSocket(clients[i]);
        client_ids[clients[i]] = i;
        LOG("Client ", i, " connecting...");
    }

    // Track which clients have been accepted and received data
    std::map<Socket*, bool> server_sockets;
    std::map<int, bool> client_received;
    std::map<int, bool> client_echoed;
    
    int frames = 0;
    while (frames < 200) { // Run for 200 frames (~2s)
        frames++;
        
        auto events = poller.poll(10);
        
        // Process listeners
        auto new_connections = listenerManager.process(events);
        for (const auto& res : new_connections) {
            server_sockets[res.new_socket] = true;
            socketManager.addSocket(res.new_socket);
            LOG("Server accepted connection (total: ", server_sockets.size(), ")");
        }
        
        // Process sockets
        auto socket_results = socketManager.process(events);
        for (const auto& res : socket_results) {
            if (res.type == SocketResult::DATA) {
                auto view = res.socket->receive();
                std::string msg(view.data, view.size);
                res.socket->clearReadBuffer();
                
                // Check if this is a client receiving echo
                if (client_ids.count(res.socket)) {
                    int client_id = client_ids[res.socket];
                    std::string expected = "Echo from client " + std::to_string(client_id);
                    
                    if (msg == expected) {
                        client_echoed[client_id] = true;
                        LOG("Client ", client_id, " received correct echo");
                    } else {
                        LOG_ERROR("Client ", client_id, " received wrong data: ", msg);
                        return 1;
                    }
                }
                // Server socket receiving from client
                else if (server_sockets.count(res.socket)) {
                    LOG("Server received: ", msg);
                    res.socket->write("Echo " + msg);
                }
            }
        }
        
        // Each client sends its unique message
        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (!client_received[i] && server_sockets.size() >= NUM_CLIENTS) {
                std::string msg = "from client " + std::to_string(i);
                clients[i]->write(msg);
                client_received[i] = true;
                LOG("Client ", i, " sent: ", msg);
            }
        }
        
        // Check if all done
        if (client_echoed.size() == NUM_CLIENTS) {
            LOG("\n✅ All ", NUM_CLIENTS, " clients received correct echoes!");
            return 0;
        }
    }

    LOG_ERROR("Test Failed: Timed out (only ", client_echoed.size(), "/", NUM_CLIENTS, " clients completed)");
    return 1;
}
