#include "poller.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    Poller poller;

    // Create and configure listener
    Listener* listener = poller.createListener();
    if (!listener) {
        std::cerr << "Failed to create listener" << std::endl;
        return 1;
    }

    // Listen on port 8080
    if (!listener->listen(8080)) {
        std::cerr << "Failed to listen on port 8080" << std::endl;
        return 1;
    }

    std::cout << "Ping-pong server listening on port 8080..." << std::endl;

    // Handle new connections
    listener->setOnAccept([](Socket* client) {
        std::cout << "New connection from " << client->remote_addr 
                  << ":" << client->remote_port << std::endl;

        // Handle incoming data from client
        client->setOnData([](Socket& socket, const std::vector<char>& data) {
            std::string message(data.begin(), data.end());
            std::cout << "Received: " << message;

            if (message.find("ping") != std::string::npos) {
                std::cout << "Sending pong response" << std::endl;
                socket.write("pong\n");
            }
        });
    });

    std::cout << "Server running... (Press Ctrl+C to stop)" << std::endl;

    // Run the event loop forever
    poller.run();

    return 0;
} 