#include "poller.hpp"
#include "websocket_client.hpp"
#include "log.hpp"
#include "steady_clock.hpp"
#include "steady_timer.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>
#include <sstream>
#include <vector>
#include <functional>

int main() {
  Poller poller;

  // Create WebSocket client
  WebSocketClient* client = WebSocketClient::fromSocket(poller.createSocket());

  if (!client) {
    LOG_ERROR("Failed to create WebSocket client");
    return 1;
  }

  LOG("WebSocket Client Example 2 - Advanced Features");
  LOG("===============================================");

  // Connection state tracking
  bool connection_established = false;
  int messages_sent = 0;
  int messages_received = 0;
  SteadyTimer session_timer;

  // Set up event handlers
  client->onOpen = [&]() {
    connection_established = true;
    LOG("🔗 WebSocket connection established!");
    LOG("Ready to send messages...");
  };

  client->onMessage = [&](const std::string& message) {
    messages_received++;
    int elapsed = session_timer.getElapsedMs();
    LOG("📨 [", elapsed, "ms] Text message #", messages_received, ": ", message);
  };

  client->onBinary = [&](const std::vector<uint8_t>& data) {
    messages_received++;
    int elapsed = session_timer.getElapsedMs();
    
    // Convert binary data to hex string for display
    std::ostringstream hex_stream;
    hex_stream << std::hex;
    for (size_t i = 0; i < std::min(data.size(), size_t(16)); ++i) {
      hex_stream << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 16) hex_stream << "...";
    
    LOG("📦 [", elapsed, "ms] Binary message #", messages_received, " (", data.size(), " bytes): ", hex_stream.str());
  };

  client->onClose = [&](uint16_t code, const std::string& reason) {
    int elapsed = session_timer.getElapsedMs();
    LOG("🔒 [", elapsed, "ms] Connection closed with code: ", code, ", reason: ", reason);
    LOG("📊 Session stats - Sent: ", messages_sent, ", Received: ", messages_received);
  };

  client->onError = [](const std::string& error) {
    LOG_ERROR("❌ WebSocket error: ", error);
  };

  // Connect to local WebSocket echo server
  LOG("🚀 Connecting to WebSocket server...");
  bool success = client->connect("ws://localhost:8765/");

  if (!success) {
    LOG_ERROR("Failed to connect to WebSocket server");
    return 1;
  }

  // Stress test scenario with high-volume messaging
  poller.setTimeout(1000, [&]() {
    if (client->status == WebSocketStatus::OPEN) {
      LOG("🎯 Starting STRESS TEST - High Volume Messaging...");
      
      // Phase 1: Rapid-fire small messages
      LOG("📡 Phase 1: Rapid-fire text messages (100 messages)...");
      auto phase1_timer = poller.setTimeout(10, [&]() {
        static int rapid_count = 0;
        
        std::function<void()> send_rapid_message = [&]() {
          if (rapid_count < 100 && client->status == WebSocketStatus::OPEN) {
            messages_sent++;
            client->sendText("Rapid message #" + std::to_string(rapid_count++) + " timestamp:" + std::to_string(session_timer.getElapsedMs()));
            
            // Schedule next message with moderate delay (avoid segfault)
            poller.setTimeout(20, send_rapid_message);
          } else {
            LOG("✅ Phase 1 completed: ", rapid_count, " rapid messages sent");
            
            // Phase 2: Large binary data stress test
            LOG("📦 Phase 2: Large binary data stress (20 chunks of 512B each)...");
            auto phase2_timer = poller.setTimeout(100, [&]() {
              static int binary_count = 0;
              
              std::function<void()> send_large_binary = [&]() {
                if (binary_count < 20 && client->status == WebSocketStatus::OPEN) {
                  messages_sent++;
                  
                  // Create 512B binary chunk with pattern
                  std::vector<uint8_t> large_binary(512);
                  for (size_t i = 0; i < large_binary.size(); i++) {
                    large_binary[i] = (binary_count * 37 + i) % 256; // Pseudo-random pattern
                  }
                  
                  client->sendBinary(large_binary);
                  binary_count++;
                  
                  // Send next chunk with reasonable delay
                  poller.setTimeout(50, send_large_binary);
                } else {
                  LOG("✅ Phase 2 completed: ", binary_count, " binary chunks sent (", (binary_count * 512), " bytes total)");
                  
                  // Phase 3: Mixed message types at high frequency
                  LOG("🔄 Phase 3: Mixed message burst (text/binary alternating, 200 messages)...");
                  auto phase3_timer = poller.setTimeout(100, [&]() {
                    static int mixed_count = 0;
                    
                    std::function<void()> send_mixed_message = [&]() {
                      if (mixed_count < 200 && client->status == WebSocketStatus::OPEN) {
                        messages_sent++;
                        
                        if (mixed_count % 2 == 0) {
                          // Send text message
                          std::string json_msg = "{\"id\":" + std::to_string(mixed_count) + 
                                                ",\"type\":\"stress_text\",\"data\":\"" + 
                                                std::string(50, 'A' + (mixed_count % 26)) + "\"}";
                          client->sendText(json_msg);
                        } else {
                          // Send binary message
                          std::vector<uint8_t> binary_msg(64);
                          for (size_t i = 0; i < binary_msg.size(); i++) {
                            binary_msg[i] = (mixed_count + i) % 256;
                          }
                          client->sendBinary(binary_msg);
                        }
                        
                        mixed_count++;
                        
                        // Fast but stable timing - send every 25ms
                        poller.setTimeout(25, send_mixed_message);
                      } else {
                        LOG("✅ Phase 3 completed: ", mixed_count, " mixed messages sent");
                        
                        // Phase 4: Large single message test
                        LOG("💥 Phase 4: Large message test (64KB text message)...");
                        poller.setTimeout(500, [&]() {
                          messages_sent++;
                          
                          // Create 64KB text message (more reasonable size)
                          std::string large_message = "{\"type\":\"large_message\",\"size\":\"64KB\",\"data\":\"";
                          const std::string pattern = "STRESS_TEST_DATA_CHUNK_";
                          while (large_message.length() < 64 * 1024 - 100) { // Leave room for JSON closing
                            large_message += pattern + std::to_string(large_message.length()) + "_";
                          }
                          large_message += "\"}";
                          
                          LOG("📤 Sending 64KB message (", large_message.length(), " bytes)...");
                          client->sendText(large_message);
                          
                          // Phase 5: Burst of medium messages
                          LOG("🚀 Phase 5: Burst test (100 medium messages)...");
                          poller.setTimeout(1000, [&]() {
                            static int burst_count = 0;
                            
                            std::function<void()> send_burst_message = [&]() {
                              if (burst_count < 100 && client->status == WebSocketStatus::OPEN) {
                                messages_sent++;
                                
                                // Create medium-sized message (512 bytes)
                                std::string burst_msg = "BURST_" + std::to_string(burst_count) + "_";
                                while (burst_msg.length() < 512) {
                                  burst_msg += "DATA_";
                                }
                                
                                client->sendText(burst_msg);
                                burst_count++;
                                
                                // Fast burst timing - send every 10ms
                                poller.setTimeout(10, send_burst_message);
                              } else {
                                LOG("✅ Phase 5 completed: ", burst_count, " burst messages sent");
                                
                                // Final phase: Status and cleanup
                                LOG("📊 Final phase: Sending status report and closing...");
                                poller.setTimeout(2000, [&]() { // Wait for responses
                                  messages_sent++;
                                  int total_time = session_timer.getElapsedMs();
                                  
                                  std::string final_status = "{\"type\":\"stress_test_complete\","
                                    "\"total_messages_sent\":" + std::to_string(messages_sent) + ","
                                    "\"total_time_ms\":" + std::to_string(total_time) + ","
                                    "\"messages_per_second\":" + std::to_string((messages_sent * 1000) / std::max(total_time, 1)) + "}";
                                  
                                  client->sendText(final_status);
                                  
                                  poller.setTimeout(1000, [&]() {
                                    LOG("🏁 STRESS TEST COMPLETED! Closing connection...");
                                    client->close(1000, "Stress test completed successfully");
                                    
                                    poller.setTimeout(1000, [&]() {
                                      poller.stop();
                                    });
                                  });
                                });
                              }
                            };
                            
                            send_burst_message();
                          });
                        });
                      }
                    };
                    
                    send_mixed_message();
                  });
                }
              };
              
              send_large_binary();
            });
          }
        };
        
        send_rapid_message();
      });
    } else {
      LOG_ERROR("❌ WebSocket not open after connection attempt, status: ", (int)client->status);
      poller.stop();
    }
  });

  // Run the event loop
  LOG("🏃 Starting event loop...");
  poller.start();

  int total_time = session_timer.getElapsedMs();
  
  LOG("🏁 WebSocket Client Example 2 completed in ", total_time, "ms");
  LOG("📈 Final stats - Sent: ", messages_sent, ", Received: ", messages_received);

  return 0;
}