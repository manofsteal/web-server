#pragma once

#include "socket.hpp"
#include <functional>
#include <map>
#include <string>

enum class HttpMethod { GET, POST, PUT, DELETE, HEAD, OPTIONS };

enum class HttpStatus { PENDING, COMPLETED, ERROR };

struct HttpRequest {
  HttpMethod method = HttpMethod::GET;
  std::string url = "";
  std::string host = "";
  uint16_t port = 80;
  std::string path = "/";
  std::map<std::string, std::string> headers = {};
  std::string body = "";
};

struct HttpResponse {
  int status_code = 0;
  std::string status_text = "";
  std::map<std::string, std::string> headers = {};
  std::string body = "";
  HttpStatus status = HttpStatus::PENDING;
  std::string error_message = "";
};

struct HttpClient {
  Socket *socket = nullptr;
  HttpRequest request = {};
  HttpResponse response = {};

  using ResponseCallback = std::function<void(HttpResponse &)>;
  ResponseCallback onResponse = [](HttpResponse &) {};

  // Constructor
  static HttpClient *fromSocket(Socket *socket);

  // HTTP methods
  bool get(const std::string &url, ResponseCallback callback);
  bool post(const std::string &url, const std::string &body,
            ResponseCallback callback);
  bool put(const std::string &url, const std::string &body,
           ResponseCallback callback);
  bool delete_(const std::string &url, ResponseCallback callback);

  // Internal methods
  bool sendRequest(HttpRequest &req, ResponseCallback callback);
  void parseUrl(const std::string &url);
  std::string buildRequest();
  void parseResponse(const std::string &data);
  void handleSocketData(const BufferView &data);
};