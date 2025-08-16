#pragma once

#include "listener.hpp"
#include "socket.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

enum class WebSocketOpcode {
  CONTINUATION = 0x0,
  TEXT = 0x1,
  BINARY = 0x2,
  CLOSE = 0x8,
  PING = 0x9,
  PONG = 0xA
};

enum class WebSocketConnectionStatus { CONNECTING, OPEN, CLOSING, CLOSED };

struct WebSocketFrame {
  bool fin = true;
  bool rsv1 = false;
  bool rsv2 = false;
  bool rsv3 = false;
  WebSocketOpcode opcode = WebSocketOpcode::TEXT;
  bool masked = false;
  uint64_t payload_length = 0;
  uint32_t masking_key = 0;
  std::vector<uint8_t> payload = {};
};

struct WebSocketConnection {
  Socket *socket = nullptr;
  WebSocketConnectionStatus status = WebSocketConnectionStatus::CONNECTING;
  std::string path = "";
  std::map<std::string, std::string> headers = {};

  using MessageCallback = std::function<void(WebSocketConnection &, const std::string &)>;
  using BinaryCallback = std::function<void(WebSocketConnection &, const std::vector<uint8_t> &)>;
  using CloseCallback = std::function<void(WebSocketConnection &, uint16_t code, const std::string &reason)>;
  using ErrorCallback = std::function<void(WebSocketConnection &, const std::string &)>;

  MessageCallback onMessage = [](WebSocketConnection &, const std::string &) {};
  BinaryCallback onBinary = [](WebSocketConnection &, const std::vector<uint8_t> &) {};
  CloseCallback onClose = [](WebSocketConnection &, uint16_t, const std::string &) {};
  ErrorCallback onError = [](WebSocketConnection &, const std::string &) {};

  // WebSocket connection methods
  void sendText(const std::string &message);
  void sendBinary(const std::vector<uint8_t> &data);
  void close(uint16_t code = 1000, const std::string &reason = "");

  // Internal methods
  void handleSocketData(const BufferView &data);
  void parseFrame(const std::vector<uint8_t> &data);
  std::vector<uint8_t> buildFrame(const std::string &message, WebSocketOpcode opcode);
  std::vector<uint8_t> buildFrame(const std::vector<uint8_t> &data, WebSocketOpcode opcode);
};

struct WebSocketServer {
  Listener *listener = nullptr;
  std::map<std::string, std::function<void(WebSocketConnection &)>> routes = {};

  using ConnectionCallback = std::function<void(WebSocketConnection &)>;
  using DisconnectionCallback = std::function<void(WebSocketConnection &)>;

  ConnectionCallback onConnection = [](WebSocketConnection &) {};
  DisconnectionCallback onDisconnection = [](WebSocketConnection &) {};

  // Static factory method
  static WebSocketServer *fromListener(Listener *listener);

  // Route handling
  void route(const std::string &path, std::function<void(WebSocketConnection &)> handler);

  // Internal methods
  void handleConnection(Socket &socket);
  void handleHttpRequest(Socket &socket, const std::string &data, WebSocketConnection *conn);
  bool parseHttpRequest(const std::string &data, std::string &method, std::string &path, std::map<std::string, std::string> &headers);
  bool isWebSocketUpgrade(const std::map<std::string, std::string> &headers);
  std::string generateAcceptKey(const std::string &key);
  std::string buildHandshakeResponse(const std::string &accept_key);
  void upgradeToWebSocket(Socket &socket, const std::string &path, const std::map<std::string, std::string> &headers, WebSocketConnection *conn);
};