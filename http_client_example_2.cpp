#include "http_client.hpp"
#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  Poller poller;

  // Create HTTP client
  HttpClient *client = HttpClient::fromSocket(poller.createSocket());
  if (!client) {
    std::cerr << "Failed to create HTTP client" << std::endl;
    return 1;
  }

  std::cout << "HTTP Client Example 2" << std::endl;
  std::cout << "====================" << std::endl;

  // Make a GET request to local HTTP server
  bool success =
      client->get("http://localhost:8080/", [](HttpResponse &response) {
        std::cout << "GET Response:" << std::endl;
        std::cout << "Status: " << response.status_code << " "
                  << response.status_text << std::endl;
        std::cout << "Headers:" << std::endl;
        for (const auto &header : response.headers) {
          std::cout << "  " << header.first << ": " << header.second
                    << std::endl;
        }
        std::cout << "Body:" << std::endl;
        std::cout << response.body << std::endl;
        std::cout << "====================" << std::endl;
      });

  if (!success) {
    std::cerr << "Failed to send GET request" << std::endl;
    return 1;
  }

  std::cout << "GET request sent successfully!" << std::endl;

  // Run the event loop
  std::thread run_thread([&poller]() { poller.start(); });

  // Wait for response
  std::this_thread::sleep_for(std::chrono::seconds(5));

  poller.stop();
  run_thread.join();

  std::cout << "HTTP Client Example 2 completed." << std::endl;

  return 0;
}