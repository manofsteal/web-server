#include "websocket_server.hpp"
#include "containers.hpp"
#include "log.hpp"
#include "poller.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <openssl/sha.h>
#include <random>

// WebSocketServer *WebSocketServer::fromListener(Listener *listener) {
//   if (listener) {

//     WebSocketServer *server = new WebSocketServer();

//     // OPTIMIZATION STRATEGY: Use object pool or stack allocation for servers
//     server->listener = listener;

//     listener->onAccept = [server](Socket *socket) {
//       LOG("[WebSocketServer] New connection accepted");
//       server->handleConnection(*socket);
//     };

//     return server;
//   }
//   return nullptr;
// }

WebSocketServer::WebSocketServer(Listener *li) {

  if (listener != li) {
    listener = li;

    listener->onAccept = [this](Socket *socket) {
      LOG("[WebSocketServer] New connection accepted");
      this->handleConnection(*socket);
    };
  }
}

void WebSocketServer::route(const String &path,
                            Function<void(WebSocketConnection &)> handler) {
  routes[path] = handler;
}

void WebSocketServer::handleConnection(Socket &socket) {
  LOG("[WebSocketServer] Handling new connection from ", socket.remote_addr,
      ":", socket.remote_port);

  // Create a WebSocketConnection and store it
  WebSocketConnection *conn =
      new WebSocketConnection(); // MEMORY ALLOCATION: WebSocketConnection
                                 // object per connection
  // OPTIMIZATION STRATEGY: Use connection pool with pre-allocated objects,
  // reuse on disconnect

  socket.onData = [this, conn](Socket &socket, const BufferView &data) {
    LOG("[WebSocketServer] Received ", data.size, " bytes from ",
        socket.remote_addr);

    if (conn && conn->status == WebSocketConnectionStatus::OPEN) {
      conn->handleSocketData(data);
    } else {
      // Handle HTTP upgrade request
      String data_str(
          data.data,
          data.size); // MEMORY ALLOCATION: string copy of incoming data
      // OPTIMIZATION STRATEGY: Use string_view to avoid copy, parse directly
      // from BufferView
      this->handleHttpRequest(socket, data_str, conn);
    }
  };
}

void WebSocketServer::handleHttpRequest(Socket &socket, const String &data,
                                        WebSocketConnection *conn) {
  LOG("[WebSocketServer] Processing HTTP request");

  String method, path; // MEMORY ALLOCATION: string objects for HTTP parsing
  StringMap<String> headers; // MEMORY ALLOCATION: map for headers storage
  // OPTIMIZATION STRATEGY: Use string_view for parsing, pre-allocated header
  // array with fixed keys

  if (!parseHttpRequest(data, method, path, headers)) {
    LOG_ERROR("[WebSocketServer] Failed to parse HTTP request");
    socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }

  LOG("[WebSocketServer] Parsed request: ", method, " ", path);

  if (method == "GET" && isWebSocketUpgrade(headers)) {
    LOG("[WebSocketServer] WebSocket upgrade request detected");
    upgradeToWebSocket(socket, path, headers, conn);
  } else {
    LOG("[WebSocketServer] Not a WebSocket upgrade request");
    socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
  }
}

bool WebSocketServer::parseHttpRequest(const String &data, String &method,
                                       String &path,
                                       StringMap<String> &headers) {
  IStringStream stream(data); // MEMORY ALLOCATION: stringstream for parsing
  String line;                // MEMORY ALLOCATION: string for line parsing
  // OPTIMIZATION STRATEGY: Use manual parsing with pointer arithmetic, avoid
  // stringstream overhead

  // Parse request line
  if (!std::getline(stream, line)) {
    return false;
  }

  IStringStream request_line(
      line);      // MEMORY ALLOCATION: stringstream for request line
  String version; // MEMORY ALLOCATION: string for HTTP version
  // OPTIMIZATION STRATEGY: Parse in-place with char* scanning, no string
  // objects needed
  if (!(request_line >> method >> path >> version)) {
    return false;
  }

  // Parse headers
  while (std::getline(stream, line) && line != "\r" && !line.empty()) {
    size_t colon_pos = line.find(':');
    if (colon_pos != String::npos) {
      String header_name = line.substr(
          0, colon_pos); // MEMORY ALLOCATION: string for header name
      String header_value = line.substr(
          colon_pos + 1); // MEMORY ALLOCATION: string for header value
      // OPTIMIZATION STRATEGY: Use string_view parsing, compare against known
      // header constants

      // Trim whitespace
      header_name.erase(0, header_name.find_first_not_of(" \t"));
      header_name.erase(header_name.find_last_not_of(" \t\r") + 1);
      header_value.erase(0, header_value.find_first_not_of(" \t"));
      header_value.erase(header_value.find_last_not_of(" \t\r") + 1);

      // Convert header name to lowercase for case-insensitive comparison
      std::transform(header_name.begin(), header_name.end(),
                     header_name.begin(), ::tolower);

      headers[header_name] = header_value;
    }
  }

  return true;
}

bool WebSocketServer::isWebSocketUpgrade(const StringMap<String> &headers) {
  auto upgrade_it = headers.find("upgrade");
  auto connection_it = headers.find("connection");
  auto key_it = headers.find("sec-websocket-key");
  auto version_it = headers.find("sec-websocket-version");

  if (upgrade_it == headers.end() || connection_it == headers.end() ||
      key_it == headers.end() || version_it == headers.end()) {
    return false;
  }

  String upgrade = upgrade_it->second;       // MEMORY ALLOCATION: string copy
  String connection = connection_it->second; // MEMORY ALLOCATION: string copy
  String version = version_it->second;       // MEMORY ALLOCATION: string copy
  // OPTIMIZATION STRATEGY: Direct string_view comparison without copies

  std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
  std::transform(connection.begin(), connection.end(), connection.begin(),
                 ::tolower);

  return upgrade == "websocket" && connection.find("upgrade") != String::npos &&
         version == "13";
}

String WebSocketServer::generateAcceptKey(const String &key) {
  String magic_string =
      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // MEMORY ALLOCATION: string
                                              // literal copy
  String combined =
      key + magic_string; // MEMORY ALLOCATION: string concatenation
  // OPTIMIZATION STRATEGY: Use const char* for magic string, pre-sized buffer
  // for concatenation

  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char *>(combined.c_str()),
       combined.length(), hash);

  // Base64 encode using same approach as WebSocket client
  static const String base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz"
                                     "0123456789+/";

  String result; // MEMORY ALLOCATION: string for base64 result
  // OPTIMIZATION STRATEGY: Pre-allocate fixed-size buffer (28 bytes for SHA1
  // base64), avoid dynamic growth
  int val = 0, valb = -6;
  for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
    val = (val << 8) + hash[i];
    valb += 8;
    while (valb >= 0) {
      result.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) {
    result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  }
  while (result.size() % 4) {
    result.push_back('=');
  }

  return result;
}

String WebSocketServer::buildHandshakeResponse(const String &accept_key) {
  StringStream ss; // MEMORY ALLOCATION: stringstream for response building
  // OPTIMIZATION STRATEGY: Use pre-allocated response buffer template,
  // sprintf-style formatting
  ss << "HTTP/1.1 101 Switching Protocols\r\n";
  ss << "Upgrade: websocket\r\n";
  ss << "Connection: Upgrade\r\n";
  ss << "Sec-WebSocket-Accept: " << accept_key << "\r\n";
  ss << "\r\n";
  return ss.str();
}

void WebSocketServer::upgradeToWebSocket(Socket &socket, const String &path,
                                         const StringMap<String> &headers,
                                         WebSocketConnection *conn) {
  auto key_it = headers.find("sec-websocket-key");
  if (key_it == headers.end()) {
    LOG_ERROR("[WebSocketServer] Missing Sec-WebSocket-Key header");
    socket.write("HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }

  String accept_key = generateAcceptKey(
      key_it->second); // MEMORY ALLOCATION: string for accept key
  String response = buildHandshakeResponse(
      accept_key); // MEMORY ALLOCATION: string for handshake response
  // OPTIMIZATION STRATEGY: Generate response directly to socket buffer, avoid
  // intermediate strings

  LOG("[WebSocketServer] Sending handshake response");
  socket.write(response);

  // Set up WebSocket connection
  conn->socket = &socket;
  conn->status = WebSocketConnectionStatus::OPEN;
  conn->path = path;       // MEMORY ALLOCATION: string copy for path storage
  conn->headers = headers; // MEMORY ALLOCATION: map copy for headers storage
  // OPTIMIZATION STRATEGY: Store only essential headers (key, protocol), use
  // string_view for path

  // Set up connection callbacks
  conn->onMessage = [this, conn](WebSocketConnection &connection,
                                 const String &message) {
    auto route_it = routes.find(connection.path);
    if (route_it != routes.end()) {
      route_it->second(connection);
    }
  };

  conn->onBinary = [this, conn](WebSocketConnection &connection,
                                const std::vector<uint8_t> &data) {
    auto route_it = routes.find(connection.path);
    if (route_it != routes.end()) {
      route_it->second(connection);
    }
  };

  conn->onClose = [this](WebSocketConnection &connection, uint16_t code,
                         const std::string &reason) {
    LOG("[WebSocketServer] Connection closed: ", code, " - ", reason);
    onDisconnection(connection);
  };

  conn->onError = [this](WebSocketConnection &connection,
                         const std::string &error) {
    LOG_ERROR("[WebSocketServer] Connection error: ", error);
  };

  LOG("[WebSocketServer] WebSocket connection established for path: ", path);
  onConnection(*conn);

  // Call route handler if exists
  auto route_it = routes.find(path);
  if (route_it != routes.end()) {
    route_it->second(*conn);
  }
}

// WebSocketConnection methods
void WebSocketConnection::sendText(const std::string &message) {
  if (status != WebSocketConnectionStatus::OPEN) {
    onError(*this, "WebSocket connection is not open");
    return;
  }

  std::vector<uint8_t> frame = buildFrame(
      message,
      WebSocketOpcode::TEXT); // MEMORY ALLOCATION: vector for frame data
  Buffer buffer;              // MEMORY ALLOCATION: Buffer object
  // OPTIMIZATION STRATEGY: Use thread-local frame buffer pool, write directly
  // to socket buffer
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);
}

void WebSocketConnection::sendBinary(const std::vector<uint8_t> &data) {
  if (status != WebSocketConnectionStatus::OPEN) {
    onError(*this, "WebSocket connection is not open");
    return;
  }

  std::vector<uint8_t> frame = buildFrame(
      data,
      WebSocketOpcode::BINARY); // MEMORY ALLOCATION: vector for frame data
  Buffer buffer;                // MEMORY ALLOCATION: Buffer object
  // OPTIMIZATION STRATEGY: Use thread-local frame buffer pool, avoid vector
  // allocation per message
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);
}

void WebSocketConnection::close(uint16_t code, const std::string &reason) {
  if (status == WebSocketConnectionStatus::CLOSED) {
    return;
  }

  status = WebSocketConnectionStatus::CLOSING;

  // Send close frame
  std::vector<uint8_t>
      close_payload; // MEMORY ALLOCATION: vector for close payload
  // OPTIMIZATION STRATEGY: Use small stack buffer (close payload is tiny: 2
  // bytes code + reason)
  close_payload.push_back((code >> 8) & 0xFF);
  close_payload.push_back(code & 0xFF);

  for (char c : reason) {
    close_payload.push_back(static_cast<uint8_t>(c));
  }

  std::vector<uint8_t> frame = buildFrame(
      close_payload,
      WebSocketOpcode::CLOSE); // MEMORY ALLOCATION: vector for close frame
  Buffer buffer;               // MEMORY ALLOCATION: Buffer object
  // OPTIMIZATION STRATEGY: Pre-built close frame template, just patch in
  // code/reason
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);

  status = WebSocketConnectionStatus::CLOSED;
  onClose(*this, code, reason);
}

void WebSocketConnection::handleSocketData(const BufferView &data) {
  LOG("[WebSocketConnection] Processing WebSocket frame data, size: ",
      data.size);
  std::vector<uint8_t> frame_data(
      data.data,
      data.data + data.size); // MEMORY ALLOCATION: vector copy of frame data
  // OPTIMIZATION STRATEGY: Parse directly from BufferView, no intermediate copy
  // needed
  parseFrame(frame_data);
}

void WebSocketConnection::parseFrame(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;

  WebSocketFrame frame; // MEMORY ALLOCATION: WebSocketFrame struct (contains
                        // vector payload)
  // OPTIMIZATION STRATEGY: Use stack-based frame parser, pre-allocate payload
  // buffer per connection

  // Parse first byte
  frame.fin = (data[0] & 0x80) != 0;
  frame.rsv1 = (data[0] & 0x40) != 0;
  frame.rsv2 = (data[0] & 0x20) != 0;
  frame.rsv3 = (data[0] & 0x10) != 0;
  frame.opcode = static_cast<WebSocketOpcode>(data[0] & 0x0F);

  // Parse second byte
  frame.masked = (data[1] & 0x80) != 0;
  uint8_t payload_len = data[1] & 0x7F;

  size_t offset = 2;

  // Parse extended payload length
  if (payload_len == 126) {
    if (data.size() < offset + 2)
      return;
    frame.payload_length = (data[offset] << 8) | data[offset + 1];
    offset += 2;
  } else if (payload_len == 127) {
    if (data.size() < offset + 8)
      return;
    frame.payload_length = 0;
    for (int i = 0; i < 8; ++i) {
      frame.payload_length = (frame.payload_length << 8) | data[offset + i];
    }
    offset += 8;
  } else {
    frame.payload_length = payload_len;
  }

  // Parse masking key
  if (frame.masked) {
    if (data.size() < offset + 4)
      return;
    frame.masking_key = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
    offset += 4;
  }

  // Parse payload
  if (data.size() < offset + frame.payload_length)
    return;

  frame.payload.resize(
      frame.payload_length); // MEMORY ALLOCATION: vector resize for payload
  // OPTIMIZATION STRATEGY: Use connection-local payload buffer, reuse across
  // frames
  for (size_t i = 0; i < frame.payload_length; ++i) {
    frame.payload[i] = data[offset + i];
    if (frame.masked) {
      frame.payload[i] ^= ((frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
    }
  }

  // Handle frame based on opcode
  switch (frame.opcode) {
  case WebSocketOpcode::TEXT: {
    std::string message(
        frame.payload.begin(),
        frame.payload.end()); // MEMORY ALLOCATION: string from vector
    // OPTIMIZATION STRATEGY: Pass payload directly as string_view to avoid copy
    onMessage(*this, message);
    break;
  }
  case WebSocketOpcode::BINARY: {
    onBinary(*this, frame.payload);
    break;
  }
  case WebSocketOpcode::CLOSE: {
    uint16_t close_code = 1000;
    std::string close_reason = ""; // MEMORY ALLOCATION: string for close reason
    // OPTIMIZATION STRATEGY: Use string_view from payload, only allocate if
    // needed for callback

    if (frame.payload.size() >= 2) {
      close_code = (frame.payload[0] << 8) | frame.payload[1];
      if (frame.payload.size() > 2) {
        close_reason = std::string(
            frame.payload.begin() + 2,
            frame.payload.end()); // MEMORY ALLOCATION: string from vector slice
        // OPTIMIZATION STRATEGY: Use string_view constructor from payload range
      }
    }

    close(close_code, close_reason);
    break;
  }
  case WebSocketOpcode::PING: {
    // Send pong response
    std::vector<uint8_t> pong_frame = buildFrame(
        frame.payload,
        WebSocketOpcode::PONG); // MEMORY ALLOCATION: vector for pong frame
    Buffer buffer;              // MEMORY ALLOCATION: Buffer object
    // OPTIMIZATION STRATEGY: Pre-built pong frame header, append payload
    // directly to socket
    buffer.append(reinterpret_cast<const char *>(pong_frame.data()),
                  pong_frame.size());
    socket->write(buffer);
    break;
  }
  case WebSocketOpcode::PONG: {
    // Handle pong (could implement ping/pong tracking here)
    break;
  }
  default:
    break;
  }
}

std::vector<uint8_t> WebSocketConnection::buildFrame(const std::string &message,
                                                     WebSocketOpcode opcode) {
  std::vector<uint8_t> data(
      message.begin(), message.end()); // MEMORY ALLOCATION: vector from string
  // OPTIMIZATION STRATEGY: Build frame directly from string data, avoid
  // intermediate vector
  return buildFrame(data, opcode);
}

std::vector<uint8_t>
WebSocketConnection::buildFrame(const std::vector<uint8_t> &data,
                                WebSocketOpcode opcode) {
  std::vector<uint8_t>
      frame; // MEMORY ALLOCATION: vector for frame construction
  // OPTIMIZATION STRATEGY: Use thread-local pre-sized frame buffer, calculate
  // exact size upfront

  // First byte: FIN + RSV + Opcode
  uint8_t first_byte = 0x80; // FIN bit set
  first_byte |= static_cast<uint8_t>(opcode);
  frame.push_back(first_byte);

  // Second byte: MASK + Payload length (server frames are not masked)
  uint8_t second_byte = 0x00; // MASK bit not set for server->client
  if (data.size() < 126) {
    second_byte |= data.size();
    frame.push_back(second_byte);
  } else if (data.size() < 65536) {
    second_byte |= 126;
    frame.push_back(second_byte);
    frame.push_back((data.size() >> 8) & 0xFF);
    frame.push_back(data.size() & 0xFF);
  } else {
    second_byte |= 127;
    frame.push_back(second_byte);
    for (int i = 7; i >= 0; --i) {
      frame.push_back((data.size() >> (i * 8)) & 0xFF);
    }
  }

  // Add payload (no masking for server->client)
  frame.insert(frame.end(), data.begin(), data.end());

  return frame;
}