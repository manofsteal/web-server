#include "poller.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    Poller poller;
    
    // Create a socket using the poller factory (automatically added to poller)
    Socket* socket = poller.createSocket();
    if (!socket) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }
    
    std::cout << "Socket created successfully!\n";
    std::cout << "Socket ID: " << socket->id << "\n";
    std::cout << "Socket file descriptor: " << socket->file_descriptor << "\n";
    
    // For now, just demonstrate that the socket was created
    // In a real example, you would connect to a server or bind/listen
    
    // Run for a short time to demonstrate the poller works
    std::thread run_thread([&poller]() {
        poller.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    poller.stop();
    run_thread.join();
    
    poller.cleanup();
    
    std::cout << "Socket example completed successfully!\n";

    return 0;
} 