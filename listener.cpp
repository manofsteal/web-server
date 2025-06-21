#include "listener.hpp"
#include "poller.hpp"
#include <arpa/inet.h>
#include <iostream>

Listener::Listener() : Pollable() {
  type = PollableType::LISTENER;
  port = 0;
}

bool Listener::start(uint16_t port) {
  this->port = port;
  file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (file_descriptor < 0)
    return false;

  int opt = 1;
  if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    close(file_descriptor);
    return false;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(file_descriptor, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(file_descriptor);
    return false;
  }

  if (::listen(file_descriptor, SOMAXCONN) < 0) {
    close(file_descriptor);
    return false;
  }

  return true;
}

void Listener::stop() {

  if (file_descriptor >= 0) {
    close(file_descriptor);
    file_descriptor = -1;
  }

  onEvent = [this](short revents) {
    if (file_descriptor >= 0) {
      struct sockaddr_in client_addr;
      socklen_t client_len = sizeof(client_addr);
      int client_fd =
          accept(file_descriptor, (struct sockaddr *)&client_addr, &client_len);
      if (client_fd >= 0) {
        Socket *client_socket = poller->createSocket();
        client_socket->file_descriptor = client_fd;
        client_socket->remote_addr = inet_ntoa(client_addr.sin_addr);
        client_socket->remote_port = ntohs(client_addr.sin_port);
        if (onAccept) {
          onAccept(client_socket);
        }
      }
    }
  };
}
