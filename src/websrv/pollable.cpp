#include "websrv/pollable.hpp"
#include "websrv/buffer_manager.hpp"
#include <poll.h>

namespace websrv {

bool Pollable::handleError(short revents) {
    // Default implementation: check for error conditions
    return (revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
}

Buffer* Pollable::getBuffer() {
    return BufferManager::instance().getWriteBuffer();
}

void Pollable::releaseBuffer(Buffer* buffer) {
    BufferManager::instance().releaseBuffer(buffer);
}

} // namespace websrv
