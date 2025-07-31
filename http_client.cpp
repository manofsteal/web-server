#include "http_client.hpp"
#include "poller.hpp"
#include "socket.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

HttpClient *HttpClient::fromSocket(Socket *socket) {
  if (socket) {

    // toA work, but asA will cause crash
    HttpClient *client = socket->userData.toA<HttpClient>();

    client->socket = socket;

    socket->onData = [client](Socket &socket, const BufferView &data) {
      std::cout << "HttpClient received data: " << data.size << " bytes"
                << std::endl;
      client->handleSocketData(data);
    };

    return client;
  }

  return nullptr;
}

bool HttpClient::get(const std::string &url, ResponseCallback callback) {
  std::cout << "Get 1 request" << std::endl;
  request.method = HttpMethod::GET;
  request.url = url;
  request.body = "";
  onResponse = callback;
  std::cout << "Get request" << std::endl;
  return sendRequest(request, callback);
}

bool HttpClient::post(const std::string &url, const std::string &body,
                      ResponseCallback callback) {
  request.method = HttpMethod::POST;
  request.url = url;
  request.body = body;
  onResponse = callback;
  return sendRequest(request, callback);
}

bool HttpClient::put(const std::string &url, const std::string &body,
                     ResponseCallback callback) {
  request.method = HttpMethod::PUT;
  request.url = url;
  request.body = body;
  onResponse = callback;
  return sendRequest(request, callback);
}

bool HttpClient::delete_(const std::string &url, ResponseCallback callback) {
  request.method = HttpMethod::DELETE;
  request.url = url;
  request.body = "";
  onResponse = callback;
  return sendRequest(request, callback);
}

bool HttpClient::sendRequest(HttpRequest &req, ResponseCallback callback) {
  // Reset response
  response = HttpResponse{};
  response.status = HttpStatus::PENDING;

  // Parse URL
  parseUrl(req.url);

  // Connect to server
  if (!socket->start(request.host, request.port)) {
    response.status = HttpStatus::ERROR;
    response.error_message = "Failed to connect to " + request.host + ":" +
                             std::to_string(request.port);
    std::cerr << "DEBUG: Failed to connect to " << request.host << ":"
              << request.port << std::endl;
    return false;
  }

  // Build and send HTTP request
  std::string http_request = buildRequest();
  socket->write(http_request);

  return true;
}

void HttpClient::parseUrl(const std::string &url) {
  request.host = "";
  request.port = 80;
  request.path = "/";

  size_t protocol_end = url.find("://");
  if (protocol_end != std::string::npos) {
    std::string protocol = url.substr(0, protocol_end);
    if (protocol == "https") {
      request.port = 443;
    }
  }

  size_t host_start = protocol_end != std::string::npos ? protocol_end + 3 : 0;
  size_t host_end = url.find('/', host_start);
  size_t port_pos = url.find(':', host_start);

  if (port_pos != std::string::npos &&
      (host_end == std::string::npos || port_pos < host_end)) {
    request.host = url.substr(host_start, port_pos - host_start);
    size_t port_start = port_pos + 1;
    size_t port_end = host_end != std::string::npos ? host_end : url.length();
    request.port = std::stoi(url.substr(port_start, port_end - port_start));
  } else {
    request.host = url.substr(host_start, host_end != std::string::npos
                                              ? host_end - host_start
                                              : url.length() - host_start);
  }

  if (host_end != std::string::npos) {
    request.path = url.substr(host_end);
  }
}

std::string HttpClient::buildRequest() {
  std::stringstream ss;

  // Method
  switch (request.method) {
  case HttpMethod::GET:
    ss << "GET ";
    break;
  case HttpMethod::POST:
    ss << "POST ";
    break;
  case HttpMethod::PUT:
    ss << "PUT ";
    break;
  case HttpMethod::DELETE:
    ss << "DELETE ";
    break;
  case HttpMethod::HEAD:
    ss << "HEAD ";
    break;
  case HttpMethod::OPTIONS:
    ss << "OPTIONS ";
    break;
  }

  // Path
  ss << request.path << " HTTP/1.1\r\n";

  // Headers
  ss << "Host: " << request.host << ":" << request.port << "\r\n";
  ss << "Connection: close\r\n";

  // Add custom headers
  for (const auto &header : request.headers) {
    ss << header.first << ": " << header.second << "\r\n";
  }

  // Add content length for POST/PUT
  if ((request.method == HttpMethod::POST ||
       request.method == HttpMethod::PUT) &&
      !request.body.empty()) {
    ss << "Content-Length: " << request.body.length() << "\r\n";
  }

  ss << "\r\n";

  // Body
  if (!request.body.empty()) {
    ss << request.body;
  }

  return ss.str();
}

void HttpClient::parseResponse(const std::string &data) {
  static std::string buffer = "";
  buffer += data;

  // Check if we have complete headers
  size_t header_end = buffer.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return; // Incomplete headers
  }

  std::string headers_str = buffer.substr(0, header_end);
  std::string body_str = buffer.substr(header_end + 4);

  // Parse status line
  size_t first_line_end = headers_str.find('\n');
  if (first_line_end != std::string::npos) {
    std::string status_line = headers_str.substr(0, first_line_end);
    size_t space1 = status_line.find(' ');
    size_t space2 = status_line.find(' ', space1 + 1);

    if (space1 != std::string::npos && space2 != std::string::npos) {
      response.status_code =
          std::stoi(status_line.substr(space1 + 1, space2 - space1 - 1));
      response.status_text = status_line.substr(space2 + 1);
    }
  }

  // Parse headers
  std::istringstream header_stream(headers_str);
  std::string line;
  bool first_line = true;

  while (std::getline(header_stream, line)) {
    if (first_line) {
      first_line = false;
      continue; // Skip status line
    }

    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);

      // Remove leading/trailing whitespace
      value.erase(0, value.find_first_not_of(" \t\r\n"));
      value.erase(value.find_last_not_of(" \t\r\n") + 1);

      response.headers[key] = value;
    }
  }

  // Set body
  response.body = body_str;
  response.status = HttpStatus::COMPLETED;

  // Call callback
  if (onResponse) {
    onResponse(response);
  }

  // Clear buffer for next request
  buffer.clear();
}

void HttpClient::handleSocketData(const BufferView &data) {
  std::string data_str(data.data, data.size);
  parseResponse(data_str);
}