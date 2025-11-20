#include "websrv/network_system.hpp"
#include "websrv/log.hpp"
#include <cassert>
#include <cstring>

using namespace websrv;

void runServer() {
    LOG("=== Running in SERVER mode ===");
    
    NetworkSystem network;
    
    // Create server
    Listener* server = network.createListener(8085);
    if (!server) {
        LOG_ERROR("Failed to start server on port 8085");
        return;
    }
    LOG("✓ Server started on port 8085");
    
    int connections_accepted = 0;
    int messages_echoed = 0;
    
    // Run server loop indefinitely
    while (true) {
        auto events = network.poll(100);
        
        for (const auto& event : events) {
            switch (event.type) {
                case NetworkEvent::ACCEPTED:
                    connections_accepted++;
                    LOG("✓ Connection accepted (total: ", connections_accepted, ")");
                    event.socket->write("Welcome from server");
                    break;
                    
                case NetworkEvent::SOCKET_DATA: {
                    auto view = event.socket->receive();
                    std::string msg(view.data, view.size);
                    event.socket->clearReadBuffer();
                    
                    LOG("✓ Received: ", msg);
                    
                    // Echo back
                    event.socket->write("Echo: " + msg);
                    messages_echoed++;
                    break;
                }
                    
                case NetworkEvent::SOCKET_CLOSED:
                    LOG("✓ Connection closed");
                    break;
                    
                case NetworkEvent::SOCKET_ERROR:
                    LOG("✗ Socket error");
                    break;
            }
        }
        
        // Clean up closed/errored sockets
        network.removeClosedSockets(events);
    }
}

void runClient() {
    LOG("=== Running in CLIENT mode ===");
    
    NetworkSystem network;
    
    // Create client
    Socket* client = network.createSocket("127.0.0.1", 8085);
    if (!client) {
        LOG_ERROR("Failed to create client");
        return;
    }
    LOG("✓ Client created, connecting to 127.0.0.1:8085");
    
    const int NUM_MESSAGES = 100;
    int current_message = 0;
    bool waiting_for_echo = false;
    
    // Send messages one at a time, wait for echo before sending next
    while (current_message < NUM_MESSAGES) {
        // Send next message if not waiting
        if (!waiting_for_echo) {
            std::string msg = "Message " + std::to_string(current_message);
            client->write(msg);
            waiting_for_echo = true;
            
            if ((current_message + 1) % 10 == 0) {
                LOG("✓ Sent ", current_message + 1, " messages");
            }
        }
        
        // Process events
        auto events = network.poll(10);
        
        for (const auto& event : events) {
            switch (event.type) {
                case NetworkEvent::SOCKET_DATA: {
                    auto view = event.socket->receive();
                    std::string msg(view.data, view.size);
                    event.socket->clearReadBuffer();
                    
                    // Handle welcome message (may be concatenated with first echo)
                    if (msg.find("Welcome from server") == 0) {
                        // Remove welcome prefix
                        if (msg.length() > 19) {
                            msg = msg.substr(19); // Remove "Welcome from server"
                        } else {
                            continue; // Just welcome, no echo yet
                        }
                    }
                    
                    // Validate echo
                    std::string expected = "Echo: Message " + std::to_string(current_message);
                    if (msg == expected) {
                        current_message++;
                        waiting_for_echo = false;
                        
                        if (current_message % 10 == 0) {
                            LOG("✓ Received ", current_message, " echoes");
                        }
                    } else {
                        LOG_ERROR("✗ Unexpected response: ", msg);
                        LOG_ERROR("   Expected: ", expected);
                        return;
                    }
                    break;
                }
                    
                case NetworkEvent::SOCKET_CLOSED:
                    LOG_ERROR("✗ Connection closed after ", current_message, " echoes");
                    return;
                
                case NetworkEvent::SOCKET_ERROR:
                    LOG_ERROR("✗ Socket error");
                    return;
            }
        }
    }
    
    LOG("\n✅ Client test passed!");
    LOG("   - Sent ", NUM_MESSAGES, " messages");
    LOG("   - Received ", NUM_MESSAGES, " correct echoes");
}

void runIntegrated() {
    LOG("=== Running INTEGRATED test (single process) ===");
    
    NetworkSystem network;

    // Create server
    Listener* server = network.createListener(8085);
    if (!server) {
        LOG_ERROR("Failed to start server on port 8085");
        return;
    }
    LOG("✓ Server started on port 8085");

    // Create client
    Socket* client = network.createSocket("127.0.0.1", 8085);
    if (!client) {
        LOG_ERROR("Failed to create client");
        return;
    }
    LOG("✓ Client created");

    bool server_accepted = false;
    bool client_received_welcome = false;
    bool server_received_echo = false;

    // Run for max 200 frames (~2 seconds)
    for (int frame = 0; frame < 200; frame++) {
        auto events = network.poll(10);
        
        for (const auto& event : events) {
            switch (event.type) {
                case NetworkEvent::ACCEPTED:
                    LOG("✓ Server accepted connection");
                    server_accepted = true;
                    event.socket->write("Welcome");
                    break;
                    
                case NetworkEvent::SOCKET_DATA: {
                    auto view = event.socket->receive();
                    std::string msg(view.data, view.size);
                    event.socket->clearReadBuffer();
                    
                    LOG("✓ Received data: ", msg);
                    
                    if (msg == "Welcome") {
                        client_received_welcome = true;
                        event.socket->write("Echo");
                    } else if (msg == "Echo") {
                        server_received_echo = true;
                    }
                    break;
                }
                    
                case NetworkEvent::SOCKET_CLOSED:
                    LOG("Connection closed");
                    break;
                    
                case NetworkEvent::SOCKET_ERROR:
                    LOG("Socket error");
                    break;
            }
        }
        
        if (server_accepted && client_received_welcome && server_received_echo) {
            LOG("\n✅ Integrated test passed!");
            LOG("   - Simple API with auto-registration");
            LOG("   - Unified event handling");
            LOG("   - Much less boilerplate!");
            return;
        }
    }

    LOG_ERROR("✗ Integrated test timed out");
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "server") == 0) {
            runServer();
            return 0;
        } else if (strcmp(argv[1], "client") == 0) {
            runClient();
            return 0;
        } else {
            LOG_ERROR("Unknown mode: ", argv[1]);
            LOG("Usage: ", argv[0], " [server|client]");
            LOG("  server - Run as echo server");
            LOG("  client - Send 100 messages and validate echoes");
            LOG("  (no args) - Run integrated test");
            return 1;
        }
    }
    
    // Default: run integrated test
    runIntegrated();
    return 0;
}
