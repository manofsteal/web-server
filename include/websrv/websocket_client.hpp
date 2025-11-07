#pragma once

#include "websrv/socket.hpp"
#include <functional>
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

enum class WebSocketStatus { CONNECTING, OPEN, CLOSING, CLOSED };

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

struct WebSocketClient {
  Socket *socket = nullptr;
  WebSocketStatus status = WebSocketStatus::CLOSED;
  std::string url = "";
  std::string host = "";
  uint16_t port = 80;
  std::string path = "/";
  std::string protocol = "";
  std::string key = "";

  using MessageCallback = std::function<void(const std::string &)>;
  using BinaryCallback = std::function<void(const std::vector<uint8_t> &)>;
  using OpenCallback = std::function<void()>;
  using CloseCallback =
      std::function<void(uint16_t code, const std::string &reason)>;
  using ErrorCallback = std::function<void(const std::string &)>;

  MessageCallback onMessage = [](const std::string &) {};
  BinaryCallback onBinary = [](const std::vector<uint8_t> &) {};
  OpenCallback onOpen = []() {};
  CloseCallback onClose = [](uint16_t, const std::string &) {};
  ErrorCallback onError = [](const std::string &) {};

  // Constructor
  static WebSocketClient *fromSocket(Socket *socket);

  // WebSocket methods
  bool connect(const std::string &url);
  void sendText(const std::string &message);
  void sendBinary(const std::vector<uint8_t> &data);
  void close(uint16_t code = 1000, const std::string &reason = "");

  // Internal methods
  bool performHandshake();
  void parseUrl(const std::string &url);
  std::string buildHandshakeRequest();
  bool parseHandshakeResponse(const std::string &data);
  void handleSocketData(const BufferView &data);
  void parseFrame(const std::vector<uint8_t> &data);
  std::vector<uint8_t>
  buildFrame(const std::string &message,
             WebSocketOpcode opcode = WebSocketOpcode::TEXT);
  std::vector<uint8_t>
  buildFrame(const std::vector<uint8_t> &data,
             WebSocketOpcode opcode = WebSocketOpcode::BINARY);
  std::string generateKey();
};