#pragma once

#include "websrv/any.hpp"
#include "websrv/buffer.hpp"
#include "websrv/buffer_view.hpp"
#include "websrv/pollable.hpp"
#include <arpa/inet.h>
#include <cstring> // for strerror
#include <errno.h> // for errno
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace websrv {

struct Socket : Pollable {

  std::string remote_addr = "";
  uint16_t remote_port = 0;

  std::vector<char> read_buffer;
  Buffer write_buffer = Buffer{};

  // for higher application protocol
  Any userData;

  Socket();

  bool start(const std::string &host, uint16_t port);
  void close();
  
  // Process I/O events
  bool handleRead();   // Returns true if data was read
  bool handleWrite();  // Returns true if data was written
  bool handleError(short revents); // Returns true if error occurred
  
  // Data access
  BufferView receive();
  void clearReadBuffer();
  void write(const std::string &data);
  void write(const char *data, size_t len);

private:
  friend class SocketManager;
};

} // namespace websrv