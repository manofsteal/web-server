#include "websrv/websocket_client.hpp"
#include "websrv/poller.hpp"
#include "websrv/log.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

struct PerfStats {
    std::atomic<int> messages_sent{0};
    std::atomic<int> messages_received{0};
    std::atomic<long long> total_latency_us{0};
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <server_url> <num_messages> <concurrent_clients>" << std::endl;
        std::cout << "Example: " << argv[0] << " ws://localhost:8765/echo 1000 10" << std::endl;
        return 1;
    }

    std::string server_url = argv[1];
    int num_messages = std::stoi(argv[2]);
    int concurrent_clients = std::stoi(argv[3]);

    LOG("WebSocket Performance Test");
    LOG("==========================");
    LOG("Server: ", server_url);
    LOG("Messages per client: ", num_messages);
    LOG("Concurrent clients: ", concurrent_clients);
    LOG("Total messages: ", num_messages * concurrent_clients);

    Poller poller;
    PerfStats stats;
    std::atomic<int> clients_connected{0};
    std::atomic<int> clients_finished{0};
    
    std::vector<WebSocketClient*> clients;
    std::vector<std::chrono::steady_clock::time_point> send_times(num_messages);

    // Create clients
    for (int i = 0; i < concurrent_clients; i++) {
        WebSocketClient* client = WebSocketClient::fromSocket(poller.createSocket());
        if (!client) {
            LOG_ERROR("Failed to create client ", i);
            continue;
        }

        clients.push_back(client);

        // Set up handlers for each client
        client->onOpen = [&stats, &clients_connected, concurrent_clients]() {
            clients_connected++;
            if (clients_connected == concurrent_clients) {
                LOG("All ", concurrent_clients, " clients connected. Starting test...");
                stats.start_time = std::chrono::steady_clock::now();
            }
        };

        client->onMessage = [&stats, &send_times, num_messages](const std::string& message) {
            stats.messages_received++;
            
            // Extract message ID to calculate latency
            if (message.find("Echo: Message ") == 0) {
                size_t start_pos = message.find("Message ") + 8;
                size_t end_pos = message.find(" ", start_pos);
                if (end_pos != std::string::npos) {
                    int msg_id = std::stoi(message.substr(start_pos, end_pos - start_pos));
                    if (msg_id >= 0 && msg_id < num_messages) {
                        auto now = std::chrono::steady_clock::now();
                        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                            now - send_times[msg_id]).count();
                        stats.total_latency_us += latency;
                    }
                }
            }
        };

        client->onError = [i](const std::string& error) {
            LOG_ERROR("Client ", i, " error: ", error);
        };

        client->onClose = [&clients_finished, &stats, concurrent_clients](uint16_t code, const std::string& reason) {
            clients_finished++;
            if (clients_finished == concurrent_clients) {
                stats.end_time = std::chrono::steady_clock::now();
                LOG("All clients finished");
            }
        };

        // Connect client
        if (!client->connect(server_url)) {
            LOG_ERROR("Failed to connect client ", i);
        }
    }

    // Wait for all clients to connect
    while (clients_connected < concurrent_clients) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Send messages from all clients
    poller.setTimeout(1000, [&]() {
        for (int msg = 0; msg < num_messages; msg++) {
            send_times[msg] = std::chrono::steady_clock::now();
            
            for (auto* client : clients) {
                if (client->status == WebSocketStatus::OPEN) {
                    std::string message = "Message " + std::to_string(msg) + " from client";
                    client->sendText(message);
                    stats.messages_sent++;
                }
            }
            
            // Small delay between message batches to avoid overwhelming
            if (msg % 100 == 0 && msg > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // Close all clients after sending
        poller.setTimeout(2000, [&]() {
            for (auto* client : clients) {
                if (client->status == WebSocketStatus::OPEN) {
                    client->close(1000, "Test complete");
                }
            }
        });
    });

    // Start poller
    poller.start();

    // Wait for test completion
    while (clients_finished < concurrent_clients) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    poller.stop();

    // Calculate and display results
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        stats.end_time - stats.start_time).count();
    
    double throughput = (double)stats.messages_received / (duration / 1000.0);
    double avg_latency = stats.messages_received > 0 ? 
        (double)stats.total_latency_us / stats.messages_received / 1000.0 : 0;

    LOG("");
    LOG("Performance Results:");
    LOG("===================");
    LOG("Messages sent: ", stats.messages_sent.load());
    LOG("Messages received: ", stats.messages_received.load());
    LOG("Test duration: ", duration, " ms");
    LOG("Throughput: ", (int)throughput, " messages/second");
    LOG("Average latency: ", avg_latency, " ms");
    LOG("Message loss: ", stats.messages_sent - stats.messages_received, 
        " (", (double)(stats.messages_sent - stats.messages_received) / stats.messages_sent * 100, "%)");

    // Cleanup
    for (auto* client : clients) {
        delete client;
    }

    return 0;
}