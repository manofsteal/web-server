#pragma once

#include "containers.hpp"
#include "listener.hpp"
#include "socket.hpp"
#include <functional>

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
  Vector<uint8_t> payload = {};
};

struct WebSocketConnection {
  Socket *socket = nullptr;
  WebSocketConnectionStatus status = WebSocketConnectionStatus::CONNECTING;
  String path = "";
  StringMap<String> headers = {};

  using MessageCallback = Function<void(WebSocketConnection &, const String &)>;
  using BinaryCallback =
      Function<void(WebSocketConnection &, const Vector<uint8_t> &)>;
  using CloseCallback = Function<void(WebSocketConnection &, uint16_t code,
                                      const String &reason)>;
  using ErrorCallback = Function<void(WebSocketConnection &, const String &)>;

  MessageCallback onMessage = [](WebSocketConnection &, const String &) {};
  BinaryCallback onBinary = [](WebSocketConnection &, const Vector<uint8_t> &) {
  };
  CloseCallback onClose = [](WebSocketConnection &, uint16_t, const String &) {
  };
  ErrorCallback onError = [](WebSocketConnection &, const String &) {};

  // WebSocket connection methods
  void sendText(const String &message);
  void sendBinary(const Vector<uint8_t> &data);
  void close(uint16_t code = 1000, const String &reason = "");

  // Internal methods
  void handleSocketData(const BufferView &data);
  void parseFrame(const Vector<uint8_t> &data);
  Vector<uint8_t> buildFrame(const String &message, WebSocketOpcode opcode);
  Vector<uint8_t> buildFrame(const Vector<uint8_t> &data,
                             WebSocketOpcode opcode);
};

struct WebSocketServer {
  Listener *listener = nullptr;
  HashMap<String, Function<void(WebSocketConnection &)>> routes = {};

  using ConnectionCallback = Function<void(WebSocketConnection &)>;
  using DisconnectionCallback = Function<void(WebSocketConnection &)>;

  ConnectionCallback onConnection = [](WebSocketConnection &) {};
  DisconnectionCallback onDisconnection = [](WebSocketConnection &) {};

  WebSocketServer() {}

  WebSocketServer(Listener *listener);

  // Route handling
  void route(const String &path, Function<void(WebSocketConnection &)> handler);

  // Internal methods
  void handleConnection(Socket &socket);
  void handleHttpRequest(Socket &socket, const String &data,
                         WebSocketConnection *conn);
  bool parseHttpRequest(const String &data, String &method, String &path,
                        StringMap<String> &headers);
  bool isWebSocketUpgrade(const StringMap<String> &headers);
  String generateAcceptKey(const String &key);
  String buildHandshakeResponse(const String &accept_key);
  void upgradeToWebSocket(Socket &socket, const String &path,
                          const StringMap<String> &headers,
                          WebSocketConnection *conn);
};