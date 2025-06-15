#include "poller.hpp"
#include "apple_timer.hpp"
#include "linux_timer.hpp"

void Poller::cleanup() {
    for (auto& [id, entry] : pollEntries) {
        entry.cleanup();
    }
    pollEntries.clear();
    poolManager.cleanup();
}

// Factory methods
Socket* Poller::createSocket() {
    Socket* socket = poolManager.sockets.create(&poolManager);
    addSocket(socket);
    return socket;
}

Timer* Poller::createTimer() {
    Timer* timer = poolManager.timers.create(&poolManager);    
    addTimer(timer);
    return timer;
}

Listener* Poller::createListener() {
    Listener* listener = poolManager.listeners.create(&poolManager);
    addListener(listener);
    return listener;
}

void Poller::addSocket(Socket* socket) {
    if (!socket) return;
    
    // Create poll entry - handle socket I/O
    PollEntry entry;
    entry.init(socket, POLLIN | POLLOUT, [socket]() {
        // Handle incoming data
        if (socket->file_descriptor >= 0) {
            char buffer[1024];
            ssize_t bytes_read = read(socket->file_descriptor, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                std::vector<char> data(buffer, buffer + bytes_read);
                if (socket->on_data) {
                    socket->on_data(*socket, data);
                }
            }
            
            // Handle outgoing data
            if (!socket->write_buffer.empty()) {
                ssize_t bytes_written = write(socket->file_descriptor, 
                                            socket->write_buffer.data(), 
                                            socket->write_buffer.size());
                if (bytes_written > 0) {
                    socket->write_buffer.erase(socket->write_buffer.begin(), 
                                             socket->write_buffer.begin() + bytes_written);
                }
            }
        }
    });
    pollEntries[socket->id] = entry;
}

void Poller::addTimer(Timer* timer) {
    if (!timer) return;
    
    // Create poll entry - when timer fd is ready, call handleExpiration
    PollEntry entry;
    entry.init(timer, POLLIN, [timer]() {
        timer->handleExpiration();
    });
    pollEntries[timer->id] = entry;
}

void Poller::addListener(Listener* listener) {
    if (!listener) return;
    
    // Create poll entry - handle new connections
    PollEntry entry;
    entry.init(listener, POLLIN, [this, listener]() {
        if (listener->file_descriptor >= 0) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listener->file_descriptor, 
                                 (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd >= 0) {
                // Create new socket for the client
                Socket* client_socket = createSocket();
                client_socket->file_descriptor = client_fd;
                client_socket->remote_addr = inet_ntoa(client_addr.sin_addr);
                client_socket->remote_port = ntohs(client_addr.sin_port);
                
                if (listener->on_accept) {
                    listener->on_accept(client_socket);
                }
            }
        }
    });
    pollEntries[listener->id] = entry;
}

void Poller::remove(PollableID id) {
    auto it = pollEntries.find(id);
    if (it != pollEntries.end()) {
        it->second.cleanup();
        pollEntries.erase(it);
    }
    
    // Remove from pools
    poolManager.timers.destroy(id);
    poolManager.sockets.destroy(id);
    poolManager.listeners.destroy(id);
}

void Poller::run() {
    running = true;
    while (running) {
        // Construct pollFds from poll entries
        pollFds.clear();
        
        for (const auto& [id, entry] : pollEntries) {
            if (entry.pollable && entry.pollable->file_descriptor >= 0) {
                pollfd pfd;
                pfd.fd = entry.pollable->file_descriptor;
                pfd.events = entry.events;
                pfd.revents = 0;
                pollFds.push_back(pfd);
            }
        }

        //TODO this will consume a lot of CPU if there are no events to wait for
        // need to find a way to sleep until an event is ready
        if (pollFds.empty()) {
            continue;
        }

        int ready = poll(pollFds.data(), pollFds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (const auto& pfd : pollFds) {
            if (pfd.revents & pfd.events) {
                // Find the corresponding poll entry and call its callback
                for (const auto& [id, entry] : pollEntries) {
                    if (entry.pollable && entry.pollable->file_descriptor == pfd.fd) {
                        if (entry.callback) {
                            entry.callback();
                        }
                        break;
                    }
                }
            }
        }
    }
}

void Poller::stop() {
    running = false;
} 