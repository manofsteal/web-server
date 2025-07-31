#include "http_server.hpp"
#include "poller.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

HttpServer *HttpServer::fromListener(Listener *listener) {
  if (listener) {
    // Create a new HttpServer object
    HttpServer *server = new HttpServer();
    server->listener = listener;

    listener->onAccept = [server](Socket *socket) {
      server->handleConnection(*socket);
    };

    return server;
  }

  return nullptr;
}

void HttpServer::get(
    const std::string &path,
    std::function<void(HttpRequest &, HttpResponse &)> handler) {
  routes["GET:" + path] = handler;
}

void HttpServer::post(
    const std::string &path,
    std::function<void(HttpRequest &, HttpResponse &)> handler) {
  routes["POST:" + path] = handler;
}

void HttpServer::put(
    const std::string &path,
    std::function<void(HttpRequest &, HttpResponse &)> handler) {
  routes["PUT:" + path] = handler;
}

void HttpServer::delete_(
    const std::string &path,
    std::function<void(HttpRequest &, HttpResponse &)> handler) {
  routes["DELETE:" + path] = handler;
}

void HttpServer::handleConnection(Socket &socket) {
  socket.onData = [this](Socket &socket, const BufferView &data) {
    std::string data_str(data.data, data.size);
    handleRequest(socket, data_str);
  };
}

void HttpServer::handleRequest(Socket &socket, const std::string &data) {
  HttpRequest request;
  HttpResponse response;

  // Parse the HTTP request
  parseRequest(data, request);

  // Set client info
  request.remote_addr = socket.remote_addr;
  request.remote_port = socket.remote_port;

  // Find and execute route handler
  std::string route_key = findRoute(request.path, request.method);
  auto it = routes.find(route_key);

  if (it != routes.end()) {
    it->second(request, response);
  } else {
    // 404 Not Found
    response.status_code = 404;
    response.status_text = "Not Found";
    response.body = "<h1>404 Not Found</h1><p>The requested resource was not "
                    "found on this server.</p>";
  }

  // Build and send response
  std::string http_response = buildResponse(response);
  socket.write(http_response);
}

void HttpServer::parseRequest(const std::string &data, HttpRequest &request) {
  std::istringstream stream(data);
  std::string line;

  // Parse request line
  if (std::getline(stream, line)) {
    std::istringstream line_stream(line);
    std::string method_str, url, version;

    if (line_stream >> method_str >> url >> version) {
      // Parse method
      if (method_str == "GET")
        request.method = HttpMethod::GET;
      else if (method_str == "POST")
        request.method = HttpMethod::POST;
      else if (method_str == "PUT")
        request.method = HttpMethod::PUT;
      else if (method_str == "DELETE")
        request.method = HttpMethod::DELETE;
      else if (method_str == "HEAD")
        request.method = HttpMethod::HEAD;
      else if (method_str == "OPTIONS")
        request.method = HttpMethod::OPTIONS;

      // Parse URL
      request.url = url;
      size_t query_pos = url.find('?');
      if (query_pos != std::string::npos) {
        request.path = url.substr(0, query_pos);
        request.query = url.substr(query_pos + 1);
      } else {
        request.path = url;
      }
    }
  }

  // Parse headers
  while (std::getline(stream, line) && line != "\r" && !line.empty()) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);

      // Remove leading/trailing whitespace and \r
      value.erase(0, value.find_first_not_of(" \t\r"));
      value.erase(value.find_last_not_of(" \t\r") + 1);

      request.headers[key] = value;
    }
  }

  // Parse body (if any)
  std::string body;
  while (std::getline(stream, line)) {
    body += line + "\n";
  }
  if (!body.empty()) {
    request.body = body.substr(0, body.length() - 1); // Remove trailing newline
  }
}

std::string HttpServer::buildResponse(const HttpResponse &response) {
  std::stringstream ss;

  // Status line
  ss << "HTTP/1.1 " << response.status_code << " " << response.status_text
     << "\r\n";

  // Headers
  for (const auto &header : response.headers) {
    ss << header.first << ": " << header.second << "\r\n";
  }

  // Content-Length header
  if (!response.body.empty()) {
    ss << "Content-Length: " << response.body.length() << "\r\n";
  }

  ss << "\r\n";

  // Body
  if (!response.body.empty()) {
    ss << response.body;
  }

  return ss.str();
}

std::string HttpServer::findRoute(const std::string &path, HttpMethod method) {
  std::string method_str;
  switch (method) {
  case HttpMethod::GET:
    method_str = "GET";
    break;
  case HttpMethod::POST:
    method_str = "POST";
    break;
  case HttpMethod::PUT:
    method_str = "PUT";
    break;
  case HttpMethod::DELETE:
    method_str = "DELETE";
    break;
  case HttpMethod::HEAD:
    method_str = "HEAD";
    break;
  case HttpMethod::OPTIONS:
    method_str = "OPTIONS";
    break;
  }

  return method_str + ":" + path;
}