#include "websrv/log.hpp"
#include "websrv/poller.hpp"
#include "websrv/steady_clock.hpp"
#include "websrv/steady_timer.hpp"
#include "websrv/websocket_client.hpp"
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc > 1 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    std::cout << "Usage: " << argv[0] << " [host] [port]" << std::endl;
    std::cout << "  host:           Server host (default: localhost)"
              << std::endl;
    std::cout << "  port:           Server port (default: 8765)" << std::endl;
    return 0;
  }

  Poller poller;

  // Test configuration parameters
  const int total_messages = 10000;     // Number of messages (hardcoded)
  const int delay_between_messages = 0; // Delay between messages (hardcoded)
  std::string host = "localhost";       // Server host
  int port = 8765;                      // Server port

  // Parse command line arguments
  if (argc >= 2) {
    host = argv[1];
  }
  if (argc >= 3) {
    port = std::atoi(argv[2]);
    if (port <= 0 || port > 65535)
      port = 8765;
  }

  // Create WebSocket client
  WebSocketClient *client = WebSocketClient::fromSocket(poller.createSocket());

  if (!client) {
    LOG_ERROR("Failed to create WebSocket client");
    return 1;
  }

  LOG("WebSocket Client Stress Test");
  LOG("============================");
  LOG("🎯 Test Parameters:");
  LOG("   Server: ", host, ":", port);
  LOG("   Total messages: ", total_messages);
  LOG("   Delay between messages: ", delay_between_messages, "ms");
  LOG("   Expected duration: ~",
      (total_messages * delay_between_messages) / 1000, " seconds");
  LOG("");

  // State variables
  int messages_sent = 0;
  int messages_received = 0;
  int expected_message_id = 0;
  bool waiting_for_response = false;
  SteadyTimer session_timer;
  SteadyTimer message_timer;

  // Set up event handlers
  client->onOpen = [&]() {
    LOG("🔗 WebSocket connection established!");
    LOG("🚀 Starting ", total_messages, " message test...");
    session_timer.reset();

    // Send first message
    messages_sent++;
    waiting_for_response = true;
    message_timer.reset();
    std::string msg = "Message #" + std::to_string(messages_sent);
    client->sendText(msg);
    LOG("📤 Sent: ", msg);
  };

  client->onMessage = [&](const std::string &message) {
    messages_received++;
    int response_time = message_timer.getElapsedMs();

    // Extract message ID from echo response
    // std::string expected =
    //     "Echo: Message #" + std::to_string(expected_message_id + 1);

    std::string expected =
        "Message #" + std::to_string(expected_message_id + 1);
    bool correct_echo = (message == expected);

    if (correct_echo) {
      expected_message_id++;
      LOG("✅ [", response_time, "ms] Correct echo #", expected_message_id, ": ",
          message);
    } else {
      LOG_ERROR("❌ Wrong echo! Expected: ", expected, ", Got: ", message);
    }

    waiting_for_response = false;

    // Check if we need to send more messages
    if (messages_sent < total_messages) {
      // Schedule next message after configurable delay
      poller.setTimeout(delay_between_messages, [&]() {
        if (client->status == WebSocketStatus::OPEN && !waiting_for_response) {
          messages_sent++;
          waiting_for_response = true;
          message_timer.reset();
          std::string msg = "Message #" + std::to_string(messages_sent);
          client->sendText(msg);

          client->sendText(msg);
          LOG("📊 sendText: ", msg);

          // Show progress every 100 messages or every 10% of total (whichever
          // is smaller)
          int progress_interval =
              std::min(100, std::max(1, total_messages / 10));
          if (messages_sent % progress_interval == 0) {
            LOG("📊 Progress: ", messages_sent, "/", total_messages,
                " messages sent (", (messages_sent * 100) / total_messages,
                "%)");
          }
        }
      });
    } else {
      // All messages sent and received
      int total_time = session_timer.getElapsedMs();
      LOG("✅ TEST COMPLETED!");
      LOG("📈 Total messages: ", messages_sent);
      LOG("📨 Echoes received: ", messages_received);
      LOG("⏱️  Total time: ", total_time, "ms");
      LOG("⚡ Messages per second: ",
          (messages_sent * 1000) / std::max(total_time, 1));
      LOG("📊 Average response time: ", total_time / std::max(messages_sent, 1),
          "ms per message");

      // Close connection
      client->close(1000, "Test completed successfully");
    }
  };

  client->onBinary = [&](const std::vector<uint8_t> &data) {
    LOG("📦 Received unexpected binary data: ", data.size(), " bytes");
  };

  client->onClose = [&](uint16_t code, const std::string &reason) {
    int total_time = session_timer.getElapsedMs();
    LOG("🔒 Connection closed - Code: ", code, ", Reason: ", reason);
    LOG("📊 Final stats - Sent: ", messages_sent,
        ", Received: ", messages_received);
    LOG("⏱️  Session duration: ", total_time, "ms");
    poller.stop();
  };

  client->onError = [](const std::string &error) {
    LOG_ERROR("❌ WebSocket error: ", error);
  };

  // Connect to WebSocket server
  std::string ws_url = "ws://" + host + ":" + std::to_string(port) + "/";
  LOG("🚀 Connecting to WebSocket server...");
  bool success = client->connect(ws_url);

  if (!success) {
    LOG_ERROR("Failed to connect to WebSocket server");
    LOG_ERROR("Make sure WebSocket echo server is running on ", host, ":",
              port);
    return 1;
  }

  // Set up timeout for the entire test (calculate based on expected duration +
  // buffer)
  int timeout_ms =
      std::max(30000, (total_messages * delay_between_messages) + 10000);
  poller.setTimeout(timeout_ms, [&]() {
    LOG_ERROR("⏰ Test timeout after ", timeout_ms / 1000, " seconds! Only ",
              messages_received, "/", messages_sent, " messages completed");
    client->close(1001, "Test timeout");
  });

  // Run the event loop
  LOG("🏃 Starting event loop...");
  poller.start();

  return 0;
}