#pragma once

#include "websrv/any.hpp"
#include "websrv/buffer.hpp"
#include "websrv/buffer_manager.hpp"
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
#include <vector>

namespace websrv {

struct Socket : Pollable {

  std::string remote_addr = "";
  uint16_t remote_port = 0;

  std::vector<Buffer*> pending_read_buffers;  // Queue of buffers with read data
  std::vector<Buffer*> pending_write_buffers; // Queue of buffers to write

  // for higher application protocol
  Any userData;

  Socket();
  ~Socket();

  bool start(const std::string &host, uint16_t port);
  void close();
  
  // Process I/O events
  bool handleRead();   // Returns true if data was read
  bool handleWrite();  // Returns true if data was written
  bool handleError(short revents); // Returns true if error occurred
  
  // Data access
  std::vector<Buffer*> read();         // Get and remove all pending read buffers (transfers ownership)
  Buffer* getReadBuffer();             // Get current read buffer (last in queue)
  void clearReadBuffer();              // Clear all read buffers
  
  // Write buffer to socket (queues buffer for writing)
  void write(Buffer* buffer);
  
  // Get write buffer for direct writing (zero-copy)
  // Returns the last pending buffer, or creates a new one if none exists
  Buffer* getWriteBuffer();

private:
  friend class SocketManager;
};

} // namespace websrv