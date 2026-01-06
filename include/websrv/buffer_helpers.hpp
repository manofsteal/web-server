#pragma once

#include "websrv/buffer.hpp"
#include <string>
#include <vector>
#include <cstring>

namespace websrv {

// Convert string to Buffer pointer (caller owns the returned Buffer)
inline Buffer* toBuffer(const std::string& str) {
    Buffer* buf = new Buffer();
    buf->append(str.data(), str.size());
    return buf;
}

// Convert const char* to Buffer pointer (caller owns the returned Buffer)
inline Buffer* toBuffer(const char* str) {
    Buffer* buf = new Buffer();
    buf->append(str, strlen(str));
    return buf;
}

// Convert vector of Buffer pointers to string (consumes and deletes buffers)
inline std::string fromBuffer(std::vector<Buffer*> buffers) {
    std::string result;
    for (Buffer* buf : buffers) {
        if (buf) {
            result.append(reinterpret_cast<const char*>(buf->data()), buf->size());
            delete buf;
        }
    }
    return result;
}

// Convert single Buffer pointer to string (does not delete buffer)
inline std::string fromBuffer(const Buffer* buf) {
    if (!buf) return "";
    return std::string(reinterpret_cast<const char*>(buf->data()), buf->size());
}

} // namespace websrv
