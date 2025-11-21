#pragma once
#include "websrv/buffer.hpp"
#include <vector>
#include <memory>
#include <cstring>
#include <string>

namespace websrv {

// Universal buffer manager for buffer pooling and reuse
// Reduces allocations by maintaining a pool of buffers
class BufferManager {
public:
    // Get the global instance
    static BufferManager& instance() {
        static BufferManager instance;
        return instance;
    }

    // Get a buffer from the pool (or create new if pool is empty)
    Buffer* getWriteBuffer() {
        if (free_buffers_.empty()) {
            // Create new buffer
            auto buffer = std::make_unique<Buffer>();
            Buffer* ptr = buffer.get();
            all_buffers_.push_back(std::move(buffer));
            return ptr;
        }
        
        // Reuse from pool
        Buffer* buffer = free_buffers_.back();
        free_buffers_.pop_back();
        return buffer;
    }

    // Release buffer back to pool for reuse
    void releaseBuffer(Buffer* buffer) {
        if (!buffer) return;
        
        // Clear the buffer and return to pool
        buffer->clear();
        free_buffers_.push_back(buffer);
    }

    // Get statistics
    size_t totalBuffers() const { return all_buffers_.size(); }
    size_t freeBuffers() const { return free_buffers_.size(); }
    size_t usedBuffers() const { return all_buffers_.size() - free_buffers_.size(); }

private:
    BufferManager() = default;
    ~BufferManager() = default;
    
    // Prevent copying
    BufferManager(const BufferManager&) = delete;
    BufferManager& operator=(const BufferManager&) = delete;

    std::vector<std::unique_ptr<Buffer>> all_buffers_;  // Owns all buffers
    std::vector<Buffer*> free_buffers_;                  // Available for reuse
    size_t total_buffers_created = 0;
    size_t peak_active_buffers = 0;
};

// Global helper functions for convenient buffer management
inline Buffer* getBuffer() {
    return BufferManager::instance().getWriteBuffer();
}

inline void releaseBuffer(Buffer* buffer) {
    BufferManager::instance().releaseBuffer(buffer);
}

// Helper functions to convert data to Buffer
inline Buffer* toBuffer(const char* str) {
    Buffer* buf = getBuffer();
    buf->append(str, std::strlen(str));
    return buf;
}

inline Buffer* toBuffer(const std::string& str) {
    Buffer* buf = getBuffer();
    buf->append(str.data(), str.size());
    return buf;
}

inline Buffer* toBuffer(const std::vector<uint8_t>& data) {
    Buffer* buf = getBuffer();
    buf->append(reinterpret_cast<const char*>(data.data()), data.size());
    return buf;
}

// Helper functions to convert Buffer to data
inline std::string fromBuffer(Buffer* buffer) {
    if (!buffer) return "";
    std::string result;
    result.reserve(buffer->size());
    for (size_t i = 0; i < buffer->size(); ++i) {
        result += buffer->getAt(i);
    }
    return result;
}

inline std::string fromBuffer(const std::vector<Buffer*>& buffers) {
    std::string result;
    for (Buffer* buf : buffers) {
        if (buf) {
            for (size_t i = 0; i < buf->size(); ++i) {
                result += buf->getAt(i);
            }
        }
    }
    return result;
}

} // namespace websrv
