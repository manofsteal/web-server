#include "websrv/http_server.hpp"
#include "websrv/log.hpp"
#include "websrv/poller.hpp"
#include "websrv/websocket_server.hpp"
#include <iostream>
#include <thread>

using namespace websrv;

// Unified HTTP and WebSocket Server Example
// This example demonstrates how to run both HTTP and WebSocket protocols on the
// same port
//
// Test with:
// HTTP routes:
//   curl http://localhost:8080/
//   curl http://localhost:8080/api/status
//
// WebSocket routes (use websocket_client_test.py -u after changing port to
// 8080):
//   ws://localhost:8080/ws/echo
//   ws://localhost:8080/ws/chat

int main() {
  LOG("Starting Unified HTTP + WebSocket Server on port 8080");

  Poller poller;

  // Create a single listener for both HTTP and WebSocket
  Listener *listener = poller.createListener();
  if (!listener) {
    LOG_ERROR("Failed to create listener");
    return 1;
  }

  // Start listening on port 8080
  if (!listener->start(8080)) {
    LOG_ERROR("Failed to listen on port 8080");
    return 1;
  }

  // Create HTTP server
  HttpServer http_server(listener);

  // Create WebSocket server from HTTP server - automatically enables upgrade
  WebSocketServer ws_server(&http_server);

  // ============= HTTP Routes =============
  LOG("Configuring HTTP routes...");

  http_server.get("/", [](HttpRequest &request, HttpResponse &response) {
    response.body =
        "<!DOCTYPE html><html><head><title>Unified Server</title></head><body>"
        "<h1>Unified HTTP + WebSocket Server</h1>"
        "<p>This server supports both HTTP and WebSocket protocols on the same "
        "port (8080).</p>"
        "<h2>HTTP Routes:</h2>"
        "<ul>"
        "<li><a href='/'>Home</a> (this page)</li>"
        "<li><a href='/api/status'>API Status</a></li>"
        "</ul>"
        "<h2>WebSocket Routes:</h2>"
        "<ul>"
        "<li>ws://localhost:8080/ws/echo - Echo server</li>"
        "<li>ws://localhost:8080/ws/chat - Chat server</li>"
        "</ul>"
        "<h2>WebSocket Test:</h2>"
        "<div id='ws-status'>Not connected</div>"
        "<button onclick='testWebSocket()'>Test WebSocket Echo</button>"
        "<div id='ws-output' style='margin-top:10px;'></div>"
        "<script>"
        "function testWebSocket() {"
        "  const ws = new WebSocket('ws://localhost:8080/ws/echo');"
        "  const status = document.getElementById('ws-status');"
        "  const output = document.getElementById('ws-output');"
        "  ws.onopen = () => {"
        "    status.textContent = 'Connected!';"
        "    ws.send('Hello from browser!');"
        "  };"
        "  ws.onmessage = (event) => {"
        "    output.innerHTML += '<p>Received: ' + event.data + '</p>';"
        "  };"
        "  ws.onerror = (error) => {"
        "    status.textContent = 'Error: ' + error;"
        "  };"
        "  ws.onclose = () => {"
        "    status.textContent = 'Disconnected';"
        "  };"
        "}"
        "</script>"
        "</body></html>";
    response.headers["Content-Type"] = "text/html";
  });

  http_server.get(
      "/api/status", [](HttpRequest &request, HttpResponse &response) {
        response.body = "{"
                        "\"status\": \"running\","
                        "\"server\": \"unified-http-websocket\","
                        "\"protocols\": [\"HTTP/1.1\", \"WebSocket\"],"
                        "\"timestamp\": \"" +
                        std::to_string(std::time(nullptr)) +
                        "\""
                        "}";
        response.headers["Content-Type"] = "application/json";
      });

  // ============= WebSocket Routes =============
  LOG("Configuring WebSocket routes...");

  // WebSocket connection/disconnection handlers
  ws_server.onConnection = [](WebSocketConnection &conn) {
    LOG("[Server] New WebSocket connection: ", conn.path);
  };

  ws_server.onDisconnection = [](WebSocketConnection &conn) {
    LOG("[Server] WebSocket disconnected: ", conn.path);
  };

  // WebSocket echo route
  ws_server.route("/ws/echo", [](WebSocketConnection &conn) {
    LOG("[WS Echo] Client connected");

    conn.onMessage = [](WebSocketConnection &connection,
                        const String &message) {
      LOG("[WS Echo] Received text: ", message);
      connection.sendText(message);
    };

    conn.onBinary = [](WebSocketConnection &connection,
                       const Vector<uint8_t> &data) {
      LOG("[WS Echo] Received binary: ", data.size(), " bytes");
      connection.sendBinary(data);
    };
  });

  // WebSocket chat route
  ws_server.route("/ws/chat", [](WebSocketConnection &conn) {
    LOG("[WS Chat] Client connected");

    conn.onMessage = [](WebSocketConnection &connection,
                        const String &message) {
      LOG("[WS Chat] Received: ", message);
      String response = "Server reply: " + message;
      connection.sendText(response);
    };
  });

  // ============= Start Server =============
  LOG("");
  LOG("==========================================================");
  LOG("Unified Server Started on port 8080");
  LOG("==========================================================");
  LOG("");
  LOG("HTTP Routes:");
  LOG("  GET  http://localhost:8080/          - Home page");
  LOG("  GET  http://localhost:8080/api/status - API status");
  LOG("");
  LOG("WebSocket Routes:");
  LOG("  ws://localhost:8080/ws/echo - Echo server");
  LOG("  ws://localhost:8080/ws/chat - Chat server");
  LOG("");
  LOG("Test with:");
  LOG("  Browser:    http://localhost:8080/");
  LOG("  curl:       curl http://localhost:8080/api/status");
  LOG("  Python WS:  python websocket_client_test.py");
  LOG("");
  LOG("Press Ctrl+C to stop");
  LOG("==========================================================");

  // Start the poller (blocking call)
  poller.start();

  return 0;
}
