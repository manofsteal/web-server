#include "websocket_client.hpp"
#include "log.hpp"
#include "poller.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <random>
#include <sstream>

WebSocketClient *WebSocketClient::fromSocket(Socket *socket) {
  if (socket) {
    WebSocketClient *client = socket->userData.toA<WebSocketClient>();
    client->socket = socket;

    socket->onData = [client](Socket &socket, const BufferView &data) {
      LOG("[WebSocket] Socket onData callback triggered with ", data.size,
          " bytes");
      client->handleSocketData(data);
    };

    return client;
  }

  return nullptr;
}

bool WebSocketClient::connect(const std::string &url) {
  LOG("[WebSocket] Starting connection to: ", url);
  this->url = url;
  status = WebSocketStatus::CONNECTING;

  parseUrl(url);
  LOG("[WebSocket] Parsed URL - Host: ", host, ", Port: ", port,
      ", Path: ", path);

  // Connect to server
  LOG("[WebSocket] Attempting socket connection to ", host, ":", port);
  if (!socket->start(host, port)) {
    LOG_ERROR("[WebSocket] Socket connection failed");
    status = WebSocketStatus::CLOSED;
    onError("Failed to connect to " + host + ":" + std::to_string(port));
    return false;
  }

  LOG("[WebSocket] Socket connected successfully");
  // Perform WebSocket handshake
  return performHandshake();
}

void WebSocketClient::sendText(const std::string &message) {
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
  LOG("[WebSocket] Performing handshake");
  std::string handshake_request = buildHandshakeRequest();
  LOG("[WebSocket] Sending handshake request:\n", handshake_request);
  socket->write(handshake_request);
  LOG("[WebSocket] Handshake request sent, waiting for response");
  return true;
}

void WebSocketClient::parseUrl(const std::string &url) {
  host = "";
  port = 80;
  path = "/";
  protocol = "";

  size_t protocol_end = url.find("://");
  if (protocol_end != std::string::npos) {
    protocol = url.substr(0, protocol_end);
    if (protocol == "wss") {
      port = 443;
    } else if (protocol == "ws") {
      port = 80;
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
  ss << "Host: " << host;
  if ((port != 80 && port != 443) || (port == 443 && protocol != "wss") ||
      (port == 80 && protocol == "wss")) {
    ss << ":" << port;
  }
  ss << "\r\n";
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

void WebSocketClient::handleSocketData(const BufferView &data) {
  LOG("[WebSocket] Received ", data.size, " bytes of data");

  if (status == WebSocketStatus::CONNECTING) {
    LOG("[WebSocket] Processing handshake response");
    // Handle handshake response
    std::string data_str(data.data, data.size);
    LOG("[WebSocket] Handshake response:\n", data_str);

    if (!parseHandshakeResponse(data_str)) {
      LOG_ERROR("[WebSocket] Handshake failed");
      status = WebSocketStatus::CLOSED;
      return;
    }
    LOG("[WebSocket] Handshake successful, connection is now OPEN");
  } else if (status == WebSocketStatus::OPEN) {
    LOG("[WebSocket] Processing WebSocket frame");
    // Handle WebSocket frames
    std::vector<uint8_t> frame_data(data.data, data.data + data.size);
    parseFrame(frame_data);
  }
}

void WebSocketClient::parseFrame(const std::vector<uint8_t> &data) {
  LOG("[WebSocket] parseFrame: Starting to parse frame with ", data.size(), " bytes");
  if (data.size() < 2) {
    LOG_ERROR("[WebSocket] parseFrame: data.size() < 2");
    return;
  }

  WebSocketFrame frame;

  // Parse first byte
  LOG("[WebSocket] parseFrame: Parsing first byte: 0x", std::hex, (int)data[0], std::dec);
  frame.fin = (data[0] & 0x80) != 0;
  frame.rsv1 = (data[0] & 0x40) != 0;
  frame.rsv2 = (data[0] & 0x20) != 0;
  frame.rsv3 = (data[0] & 0x10) != 0;
  frame.opcode = static_cast<WebSocketOpcode>(data[0] & 0x0F);
  LOG("[WebSocket] parseFrame: FIN=", frame.fin, ", RSV1=", frame.rsv1, ", RSV2=", frame.rsv2, ", RSV3=", frame.rsv3, ", opcode=", (int)frame.opcode);

  // Parse second byte
  LOG("[WebSocket] parseFrame: Parsing second byte: 0x", std::hex, (int)data[1], std::dec);
  frame.masked = (data[1] & 0x80) != 0;
  uint8_t payload_len = data[1] & 0x7F;
  LOG("[WebSocket] parseFrame: masked=", frame.masked, ", payload_len=", (int)payload_len);

  size_t offset = 2;

  // Parse extended payload length
  if (payload_len == 126) {
    LOG("[WebSocket] parseFrame: Using 16-bit extended payload length");
    if (data.size() < offset + 2) {
      LOG_ERROR("[WebSocket] parseFrame: Early return - insufficient data for 16-bit payload length (need ", offset + 2, ", have ", data.size(), ")");
      return;
    }
    frame.payload_length = (data[offset] << 8) | data[offset + 1];
    LOG("[WebSocket] parseFrame: Extended payload length (16-bit): ", frame.payload_length);
    offset += 2;
  } else if (payload_len == 127) {
    LOG("[WebSocket] parseFrame: Using 64-bit extended payload length");
    if (data.size() < offset + 8) {
      LOG_ERROR("[WebSocket] parseFrame: Early return - insufficient data for 64-bit payload length (need ", offset + 8, ", have ", data.size(), ")");
      return;
    }
    frame.payload_length = 0;
    for (int i = 0; i < 8; ++i) {
      frame.payload_length = (frame.payload_length << 8) | data[offset + i];
    }
    LOG("[WebSocket] parseFrame: Extended payload length (64-bit): ", frame.payload_length);
    offset += 8;
  } else {
    frame.payload_length = payload_len;
    LOG("[WebSocket] parseFrame: Simple payload length: ", frame.payload_length);
  }

  // Parse masking key
  if (frame.masked) {
    LOG("[WebSocket] parseFrame: Frame is masked, parsing masking key");
    if (data.size() < offset + 4) {
      LOG_ERROR("[WebSocket] parseFrame: Early return - insufficient data for masking key (need ", offset + 4, ", have ", data.size(), ")");
      return;
    }
    frame.masking_key = (data[offset] << 24) | (data[offset + 1] << 16) |
                        (data[offset + 2] << 8) | data[offset + 3];
    LOG("[WebSocket] parseFrame: Masking key: 0x", std::hex, frame.masking_key, std::dec);
    offset += 4;
  } else {
    LOG("[WebSocket] parseFrame: Frame is not masked");
  }

  // Parse payload
  LOG("[WebSocket] parseFrame: Checking payload size - need ", offset + frame.payload_length, " bytes, have ", data.size());
  if (data.size() < offset + frame.payload_length) {
    LOG_ERROR("[WebSocket] parseFrame: Early return - insufficient data for payload (need ", offset + frame.payload_length, ", have ", data.size(), ")");
    return;
  }

  LOG("[WebSocket] parseFrame: Parsing payload of ", frame.payload_length, " bytes");
  frame.payload.resize(frame.payload_length);
  for (size_t i = 0; i < frame.payload_length; ++i) {
    frame.payload[i] = data[offset + i];
    if (frame.masked) {
      frame.payload[i] ^= ((frame.masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
    }
  }
  if (frame.masked) {
    LOG("[WebSocket] parseFrame: Payload unmasked successfully");
  }

  // Handle frame based on opcode
  LOG("[WebSocket] parseFrame: Handling opcode ", (int)frame.opcode);
  switch (frame.opcode) {
  case WebSocketOpcode::TEXT: {
    std::string message(frame.payload.begin(), frame.payload.end());
    LOG("[WebSocket] parseFrame: TEXT message received: \"", message, "\"");
    onMessage(message);
    break;
  }
  case WebSocketOpcode::BINARY: {
    LOG("[WebSocket] parseFrame: BINARY message received with ", frame.payload.size(), " bytes");
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
    LOG("[WebSocket] parseFrame: CLOSE frame received - code: ", close_code, ", reason: \"", close_reason, "\"");

    close(close_code, close_reason);
    break;
  }
  case WebSocketOpcode::PING: {
    LOG("[WebSocket] parseFrame: PING frame received with ", frame.payload.size(), " bytes payload");
    // Send pong response
    std::vector<uint8_t> pong_frame =
        buildFrame(frame.payload, WebSocketOpcode::PONG);
    Buffer buffer;
    buffer.append(reinterpret_cast<const char *>(pong_frame.data()),
                  pong_frame.size());
    socket->write(buffer);
    LOG("[WebSocket] parseFrame: PONG response sent");
    break;
  }
  case WebSocketOpcode::PONG: {
    LOG("[WebSocket] parseFrame: PONG frame received with ", frame.payload.size(), " bytes payload");
    // Handle pong (could implement ping/pong tracking here)
    break;
  }
  default:
    LOG("[WebSocket] parseFrame: Unknown opcode ", (int)frame.opcode);
    break;
  }
  LOG("[WebSocket] parseFrame: Frame processing completed");
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
  uint8_t second_byte = 0x80; // MASK bit set for client->server
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

  // Generate masking key
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  uint32_t masking_key = 0;
  for (int i = 0; i < 4; ++i) {
    uint8_t key_byte = dis(gen);
    masking_key = (masking_key << 8) | key_byte;
    frame.push_back(key_byte);
  }

  // Mask and add payload
  for (size_t i = 0; i < data.size(); ++i) {
    uint8_t masked_byte =
        data[i] ^ ((masking_key >> ((3 - (i % 4)) * 8)) & 0xFF);
    frame.push_back(masked_byte);
  }

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

  // Base64 encode using standard encoding
  static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "abcdefghijklmnopqrstuvwxyz"
                                          "0123456789+/";

  std::string result;
  int val = 0, valb = -6;
  for (uint8_t c : key_bytes) {
    val = (val << 8) + c;
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