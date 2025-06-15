#include "poller.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    Poller poller;
    
    // Create a timer using the poller factory (automatically added to poller)
    Timer* timer1 = poller.createTimer();
    if (!timer1) {
        std::cerr << "Failed to create timer1\n";
        return 1;
    }
    
    // Use setTimeout API
    if (!timer1->setTimeout(2000, []() {
        std::cout << "Timer fired!\n";
    })) {
        std::cerr << "Failed to start timer1\n";
        return 1;
    }

    // Run for 5 seconds
    std::thread run_thread([&poller]() {
        poller.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    poller.stop();
    run_thread.join();
    
    poller.cleanup();

    return 0;
} 