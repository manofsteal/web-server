#pragma once

#include "listener.hpp"
#include "socket.hpp"
#include <functional>
#include <map>
#include <string>

enum class HttpMethod { GET, POST, PUT, DELETE, HEAD, OPTIONS };

enum class HttpStatus { PENDING, COMPLETED, ERROR };

struct HttpRequest {
  HttpMethod method = HttpMethod::GET;
  std::string url = "";
  std::string path = "/";
  std::string query = "";
  std::map<std::string, std::string> headers = {};
  std::string body = "";
  std::string remote_addr = "";
  uint16_t remote_port = 0;
};

struct HttpResponse {
  int status_code = 200;
  std::string status_text = "OK";
  std::map<std::string, std::string> headers = {};
  std::string body = "";
  HttpStatus status = HttpStatus::PENDING;
  std::string error_message = "";
};

struct HttpServer {
  Listener *listener = nullptr;
  std::map<std::string, std::function<void(HttpRequest &, HttpResponse &)>>
      routes = {};

  // Constructors
  HttpServer() {}
  HttpServer(Listener *listener);

  // Route registration
  void get(const std::string &path,
           std::function<void(HttpRequest &, HttpResponse &)> handler);
  void post(const std::string &path,
            std::function<void(HttpRequest &, HttpResponse &)> handler);
  void put(const std::string &path,
           std::function<void(HttpRequest &, HttpResponse &)> handler);
  void delete_(const std::string &path,
               std::function<void(HttpRequest &, HttpResponse &)> handler);

  // Internal methods
  void handleConnection(Socket &socket);
  void handleRequest(Socket &socket, const std::string &data);
  void parseRequest(const std::string &data, HttpRequest &request);
  std::string buildResponse(const HttpResponse &response);
  std::string findRoute(const std::string &path, HttpMethod method);
};