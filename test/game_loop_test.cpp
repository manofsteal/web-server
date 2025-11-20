#include "websrv/poller.hpp"
#include "websrv/listener_manager.hpp"
#include "websrv/socket_manager.hpp"
#include "websrv/log.hpp"
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>

using namespace websrv;

int main() {
    Poller poller;
    ListenerManager listenerManager(poller);
    SocketManager socketManager;

    // Create server
    Listener* server = poller.createListener();
    if (!server->start(8080)) {
        LOG_ERROR("Failed to start server on port 8080");
        return 1;
    }
    listenerManager.addListener(server);
    LOG("Server started on port 8080");

    // Create client
    Socket* client = poller.createSocket();
    if (!client->start("127.0.0.1", 8080)) {
        // Connect might be async
    }
    socketManager.addSocket(client);
    LOG("Client started connecting...");

    bool client_connected = false;
    bool server_accepted = false;
    bool data_received = false;
    bool data_echoed = false;

    int frames = 0;
    while (frames < 100) { // Run for 100 frames (~1s)
        frames++;
        
        // 1. Poll
        auto pollerEvents = poller.poll(10);
        
        // 2. Process Listeners (pass all events, manager will filter)
        auto new_connections = listenerManager.process(pollerEvents);
        for (const auto& res : new_connections) {
            LOG("Server accepted connection");
            server_accepted = true;
            socketManager.addSocket(res.new_socket);
            res.new_socket->write("Welcome");
        }
        
        // 3. Process Sockets (pass all events, manager will filter)
        auto socket_results = socketManager.process(pollerEvents);
        for (const auto& res : socket_results) {
            if (res.type == SocketResult::DATA) {
                auto view = res.socket->receive();
                std::string msg(view.data, view.size);
                LOG("Received data: ", msg);
                res.socket->clearReadBuffer();
                
                if (msg == "Welcome") {
                    client_connected = true;
                    res.socket->write("Echo");
                } else if (msg == "Echo") {
                    data_received = true;
                    data_echoed = true;
                }
            } else if (res.type == SocketResult::CLOSED) {
                LOG("Socket closed");
            } else if (res.type == SocketResult::ERROR) {
                LOG("Socket error");
            }
        }
        
        if (server_accepted && client_connected && data_echoed) {
            LOG("Test Passed!");
            return 0;
        }
    }

    LOG_ERROR("Test Failed: Timed out");
    return 1;
}
