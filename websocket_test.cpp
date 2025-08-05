#include "websocket_client.hpp"
#include "poller.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "Testing WebSocket Client..." << std::endl;
    
    Poller poller;
    
    // Test 1: Create WebSocket client
    WebSocketClient* client = WebSocketClient::fromSocket(poller.createSocket());
    assert(client != nullptr);
    std::cout << "✓ WebSocket client creation test passed" << std::endl;
    
    // Test 2: Test key generation
    std::string key1 = client->generateKey();
    std::string key2 = client->generateKey();
    assert(key1.length() > 0);
    assert(key2.length() > 0);
    assert(key1 != key2); // Keys should be different
    std::cout << "✓ Key generation test passed (key1: " << key1 << ")" << std::endl;
    
    // Test 3: Test URL parsing
    client->parseUrl("ws://localhost:8080/test");
    assert(client->host == "localhost");
    assert(client->port == 8080);
    assert(client->path == "/test");
    std::cout << "✓ URL parsing test passed" << std::endl;
    
    // Test 4: Test URL parsing with default port
    client->parseUrl("ws://example.com/api");
    assert(client->host == "example.com");
    assert(client->port == 80);
    assert(client->path == "/api");
    std::cout << "✓ URL parsing with default port test passed" << std::endl;
    
    // Test 5: Test HTTPS URL parsing
    client->parseUrl("wss://secure.example.com:443/socket");
    assert(client->host == "secure.example.com");
    assert(client->port == 443);
    assert(client->path == "/socket");
    std::cout << "✓ HTTPS URL parsing test passed" << std::endl;
    
    // Test 6: Test handshake request building
    client->parseUrl("ws://test.com:9001/chat");
    std::string handshake = client->buildHandshakeRequest();
    assert(handshake.find("GET /chat HTTP/1.1") != std::string::npos);
    assert(handshake.find("Host: test.com:9001") != std::string::npos);
    assert(handshake.find("Upgrade: websocket") != std::string::npos);
    assert(handshake.find("Sec-WebSocket-Key:") != std::string::npos);
    std::cout << "✓ Handshake request building test passed" << std::endl;
    
    // Test 7: Test frame building
    std::string message = "Hello WebSocket";
    std::vector<uint8_t> frame = client->buildFrame(message, WebSocketOpcode::TEXT);
    assert(frame.size() > message.length()); // Frame should be larger than message
    assert((frame[0] & 0x80) != 0); // FIN bit should be set
    assert((frame[0] & 0x0F) == static_cast<uint8_t>(WebSocketOpcode::TEXT)); // Opcode should be TEXT
    std::cout << "✓ Frame building test passed" << std::endl;
    
    std::cout << "\nAll tests passed! ✓" << std::endl;
    std::cout << "WebSocket client is working correctly." << std::endl;
    
    return 0;
}