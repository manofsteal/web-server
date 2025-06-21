#include "websocket_client.hpp"
#include "poller.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <random>
#include <sstream>

WebSocketClient *WebSocketClient::fromSocket(Socket *socket) {
  if (socket) {
    WebSocketClient *client = socket->userData.toA<WebSocketClient>();
    client->socket = socket;

    socket->onData = [client](Socket &socket, const Buffer &data) {
      std::cout << "WebSocketClient received data: " << data.size() << " bytes"
                << std::endl;
      client->handleSocketData(data);
    };

    return client;
  }

  return nullptr;
}

bool WebSocketClient::connect(const std::string &url) {
  this->url = url;
  status = WebSocketStatus::CONNECTING;

  parseUrl(url);

  // Connect to server
  if (!socket->start(host, port)) {
    status = WebSocketStatus::CLOSED;
    onError("Failed to connect to " + host + ":" + std::to_string(port));
    return false;
  }

  // Perform WebSocket handshake
  return performHandshake();
}

void WebSocketClient::send(const std::string &message) {
  if (status != WebSocketStatus::OPEN) {
    onError("WebSocket is not open");
    return;
  }

  std::vector<uint8_t> frame = buildFrame(message, WebSocketOpcode::TEXT);
  Buffer buffer;
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);
}

void WebSocketClient::sendBinary(const std::vector<uint8_t> &data) {
  if (status != WebSocketStatus::OPEN) {
    onError("WebSocket is not open");
    return;
  }

  std::vector<uint8_t> frame = buildFrame(data, WebSocketOpcode::BINARY);
  Buffer buffer;
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);
}

void WebSocketClient::close(uint16_t code, const std::string &reason) {
  if (status == WebSocketStatus::CLOSED) {
    return;
  }

  status = WebSocketStatus::CLOSING;

  // Send close frame
  std::vector<uint8_t> close_payload;
  close_payload.push_back((code >> 8) & 0xFF);
  close_payload.push_back(code & 0xFF);

  for (char c : reason) {
    close_payload.push_back(static_cast<uint8_t>(c));
  }

  std::vector<uint8_t> frame =
      buildFrame(close_payload, WebSocketOpcode::CLOSE);
  Buffer buffer;
  buffer.append(reinterpret_cast<const char *>(frame.data()), frame.size());
  socket->write(buffer);

  status = WebSocketStatus::CLOSED;
  onClose(code, reason);
}

bool WebSocketClient::performHandshake() {
  std::string handshake_request = buildHandshakeRequest();
  socket->write(handshake_request);
  return true;
}

void WebSocketClient::parseUrl(const std::string &url) {
  host = "";
  port = 80;
  path = "/";
  protocol = "";

  size_t protocol_end = url.find("://");
  if (protocol_end != std::string::npos) {
    std::string protocol_str = url.substr(0, protocol_end);
    if (protocol_str == "wss") {
      port = 443;
    }
  }

  size_t host_start = protocol_end != std::string::npos ? protocol_end + 3 : 0;
  size_t host_end = url.find('/', host_start);
  size_t port_pos = url.find(':', host_start);

  if (port_pos != std::string::npos &&
      (host_end == std::string::npos || port_pos < host_end)) {
    host = url.substr(host_start, port_pos - host_start);
    size_t port_start = port_pos + 1;
    size_t port_end = host_end != std::string::npos ? host_end : url.length();
    port = std::stoi(url.substr(port_start, port_end - port_start));
  } else {
    host = url.substr(host_start, host_end != std::string::npos
                                      ? host_end - host_start
                                      : url.length() - host_start);
  }

  if (host_end != std::string::npos) {
    path = url.substr(host_end);
  }
}

std::string WebSocketClient::buildHandshakeRequest() {
  key = generateKey();

  std::stringstream ss;
  ss << "GET " << path << " HTTP/1.1\r\n";
  ss << "Host: " << host << ":" << port << "\r\n";
  ss << "Upgrade: websocket\r\n";
  ss << "Connection: Upgrade\r\n";
  ss << "Sec-WebSocket-Key: " << key << "\r\n";
  ss << "Sec-WebSocket-Version: 13\r\n";
  if (!protocol.empty()) {
    ss << "Sec-WebSocket-Protocol: " << protocol << "\r\n";
  }
  ss << "\r\n";

  return ss.str();
}

bool WebSocketClient::parseHandshakeResponse(const std::string &data) {
  std::istringstream stream(data);
  std::string line;

  // Check status line
  if (std::getline(stream, line)) {
    if (line.find("101") == std::string::npos) {
      onError("Invalid handshake response: " + line);
      return false;
    }
  }

  // Check for required headers
  bool upgrade_ok = false;
  bool connection_ok = false;
  bool accept_ok = false;

  while (std::getline(stream, line) && line != "\r" && !line.empty()) {
    if (line.find("Upgrade: websocket") != std::string::npos) {
      upgrade_ok = true;
    } else if (line.find("Connection: Upgrade") != std::string::npos) {
      connection_ok = true;
    } else if (line.find("Sec-WebSocket-Accept:") != std::string::npos) {
      accept_ok = true;
    }
  }

  if (!upgrade_ok || !connection_ok || !accept_ok) {
    onError("Missing required headers in handshake response");
    return false;
  }

  status = WebSocketStatus::OPEN;
  onOpen();
  return true;
}

void WebSocketClient::handleSocketData(const Buffer &data) {
  if (status == WebSocketStatus::CONNECTING) {
    // Handle handshake response
    std::string data_str;
    for (size_t i = 0; i < data.size(); ++i) {
      data_str += data.getAt(i);
    }

    if (!parseHandshakeResponse(data_str)) {
      status = WebSocketStatus::CLOSED;
      return;
    }
  } else if (status == WebSocketStatus::OPEN) {
    // Handle WebSocket frames
    std::vector<uint8_t> frame_data;
    for (size_t i = 0; i < data.size(); ++i) {
      frame_data.push_back(data.getAt(i));
    }
    parseFrame(frame_data);
  }
}

void WebSocketClient::parseFrame(const std::vector<uint8_t> &data) {
  if (data.size() < 2)
    return;

  WebSocketFrame frame;

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

  frame.payload.resize(frame.payload_length);
  for (size_t i = 0; i < frame.payload_length; ++i) {
    frame.payload[i] = data[offset + i];
    if (frame.masked) {
      frame.payload[i] ^= ((frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
    }
  }

  // Handle frame based on opcode
  switch (frame.opcode) {
  case WebSocketOpcode::TEXT: {
    std::string message(frame.payload.begin(), frame.payload.end());
    onMessage(message);
    break;
  }
  case WebSocketOpcode::BINARY: {
    onBinary(frame.payload);
    break;
  }
  case WebSocketOpcode::CLOSE: {
    uint16_t close_code = 1000;
    std::string close_reason = "";

    if (frame.payload.size() >= 2) {
      close_code = (frame.payload[0] << 8) | frame.payload[1];
      if (frame.payload.size() > 2) {
        close_reason =
            std::string(frame.payload.begin() + 2, frame.payload.end());
      }
    }

    close(close_code, close_reason);
    break;
  }
  case WebSocketOpcode::PING: {
    // Send pong response
    std::vector<uint8_t> pong_frame =
        buildFrame(frame.payload, WebSocketOpcode::PONG);
    Buffer buffer;
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

std::vector<uint8_t> WebSocketClient::buildFrame(const std::string &message,
                                                 WebSocketOpcode opcode) {
  std::vector<uint8_t> data(message.begin(), message.end());
  return buildFrame(data, opcode);
}

std::vector<uint8_t>
WebSocketClient::buildFrame(const std::vector<uint8_t> &data,
                            WebSocketOpcode opcode) {
  std::vector<uint8_t> frame;

  // First byte: FIN + RSV + Opcode
  uint8_t first_byte = 0x80; // FIN bit set
  first_byte |= static_cast<uint8_t>(opcode);
  frame.push_back(first_byte);

  // Second byte: MASK + Payload length
  uint8_t second_byte = 0x00; // No mask for client->server
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

  // Payload
  frame.insert(frame.end(), data.begin(), data.end());

  return frame;
}

std::string WebSocketClient::generateKey() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  std::vector<uint8_t> key_bytes(16);
  for (int i = 0; i < 16; ++i) {
    key_bytes[i] = dis(gen);
  }

  // Base64 encode
  BIO *bio, *b64;
  BUF_MEM *bufferPtr;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, key_bytes.data(), key_bytes.size());
  BIO_flush(bio);
  BIO_get_mem_ptr(bio, &bufferPtr);

  std::string result(bufferPtr->data, bufferPtr->length);

  BIO_free_all(bio);

  return result;
}