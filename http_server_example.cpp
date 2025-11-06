#include "http_server.hpp"
#include "poller.hpp"
#include <chrono>
#include <iostream>
#include <thread>

// Run this in another terminal
// curl http://localhost:8080/
// curl http://localhost:8080/hello
// curl http://localhost:8080/json
// curl -X POST http://localhost:8080/echo -d 'test body'
// curl http://localhost:8080/status

int main() {
  Poller poller;

  Listener *listener = poller.createListener();
  if (!listener) {
    std::cerr << "Failed to create listener" << std::endl;
    return 1;
  }

  HttpServer server(listener);

  std::cout << "HTTP Server Example" << std::endl;
  std::cout << "===================" << std::endl;

  // Register routes
  server.get("/", [](HttpRequest &request, HttpResponse &response) {
    response.body = "<h1>Welcome to HTTP Server</h1>"
                    "<p>This is the home page.</p>"
                    "<ul>"
                    "<li><a href='/hello'>Hello</a></li>"
                    "<li><a href='/json'>JSON API</a></li>"
                    "<li><a href='/echo'>Echo</a></li>"
                    "</ul>";
    response.headers["Content-Type"] = "text/html";
  });

  server.get("/hello", [](HttpRequest &request, HttpResponse &response) {
    response.body = "<h1>Hello, World!</h1>"
                    "<p>Hello from the HTTP server!</p>"
                    "<p>Client: " +
                    request.remote_addr + ":" +
                    std::to_string(request.remote_port) + "</p>";
    response.headers["Content-Type"] = "text/html";
  });

  server.get("/json", [](HttpRequest &request, HttpResponse &response) {
    response.body = "{"
                    "\"message\": \"Hello from JSON API\","
                    "\"timestamp\": \"" +
                    std::to_string(std::time(nullptr)) +
                    "\","
                    "\"client\": \"" +
                    request.remote_addr + ":" +
                    std::to_string(request.remote_port) +
                    "\""
                    "}";
    response.headers["Content-Type"] = "application/json";
  });

  server.post("/echo", [](HttpRequest &request, HttpResponse &response) {
    response.body = "<h1>Echo Response</h1>"
                    "<h2>Request Body:</h2>"
                    "<pre>" +
                    request.body +
                    "</pre>"
                    "<h2>Headers:</h2>"
                    "<ul>";

    for (const auto &header : request.headers) {
      response.body += "<li><strong>" + header.first + ":</strong> " +
                       header.second + "</li>";
    }

    response.body += "</ul>";
    response.headers["Content-Type"] = "text/html";
  });

  server.get("/status", [](HttpRequest &request, HttpResponse &response) {
    response.body = "<h1>Server Status</h1>"
                    "<p>Server is running and healthy!</p>"
                    "<p>Request path: " +
                    request.path +
                    "</p>"
                    "<p>Request method: " +
                    request.url + "</p>";
    response.headers["Content-Type"] = "text/html";
  });

  // Start the server on port 8080
  if (!listener->start(8080)) {
    std::cerr << "Failed to start HTTP server on port 8080" << std::endl;
    return 1;
  }

  std::cout << "HTTP server started on port 8080" << std::endl;
  std::cout << "Available routes:" << std::endl;
  std::cout << "  GET  /       - Home page" << std::endl;
  std::cout << "  GET  /hello  - Hello page" << std::endl;
  std::cout << "  GET  /json   - JSON API" << std::endl;
  std::cout << "  POST /echo   - Echo request body" << std::endl;
  std::cout << "  GET  /status - Server status" << std::endl;
  std::cout << std::endl;
  std::cout << "Test with: curl http://localhost:8080/" << std::endl;

  // Run the event loop
  std::thread run_thread([&poller]() { poller.start(); });

  // Keep the server running
  std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

  // Wait for user to stop
  std::this_thread::sleep_for(std::chrono::seconds(60));

  std::cout << "Stopping server..." << std::endl;
  poller.stop();
  run_thread.join();

  std::cout << "HTTP Server Example completed." << std::endl;

  return 0;
}