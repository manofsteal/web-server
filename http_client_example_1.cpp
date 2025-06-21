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

  std::cout << "HTTP Client Example" << std::endl;
  std::cout << "===================" << std::endl;

  // Make a GET request to httpbin.org
  bool success =
      client->get("http://httpbin.org/get", [](HttpResponse &response) {
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
        std::cout << "===================" << std::endl;
      });

  std::cout << "HTTP Client getting" << std::endl;

  if (!success) {
    std::cerr << "Failed to send GET request" << std::endl;
    return 1;
  }

  std::cout << "GET request sent successfully!" << std::endl;

  // Make a POST request
  success = client->post("http://httpbin.org/post", "{\"test\": \"data\"}",
                         [](HttpResponse &response) {
                           std::cout << "POST Response:" << std::endl;
                           std::cout << "Status: " << response.status_code
                                     << " " << response.status_text
                                     << std::endl;
                           std::cout << "Body:" << std::endl;
                           std::cout << response.body << std::endl;
                           std::cout << "===================" << std::endl;
                         });

  if (!success) {
    std::cerr << "Failed to send POST request" << std::endl;
    return 1;
  }

  std::cout << "POST request sent successfully!" << std::endl;
  std::cout << "Requests sent. Running event loop for 10 seconds..."
            << std::endl;

  // Run the event loop
  std::thread run_thread([&poller]() { poller.start(); });

  // Wait for responses
  std::this_thread::sleep_for(std::chrono::seconds(10));

  poller.stop();
  run_thread.join();

  std::cout << "HTTP Client Example completed." << std::endl;

  return 0;
}