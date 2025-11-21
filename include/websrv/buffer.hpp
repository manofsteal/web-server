#pragma once
#include <cstdint>
#include <list>
#include <vector>

namespace websrv {

class Buffer {
public:
    using value_type = uint8_t;
    using container_type = std::vector<value_type>;

private:
    container_type data_;

public:
    // Append data
    void append(const char* data, size_t len) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
        data_.insert(data_.end(), ptr, ptr + len);
    }

    void append(const uint8_t* data, size_t len) {
        data_.insert(data_.end(), data, data + len);
    }

    template <typename Container>
    void append(const Container& container) {
        const auto* ptr = reinterpret_cast<const uint8_t*>(container.data());
        data_.insert(data_.end(), ptr, ptr + container.size());
    }

    // Get byte at position
    char getAt(size_t pos) const {
        if (pos < data_.size()) {
            return static_cast<char>(data_[pos]);
        }
        return 0;
    }

    // Direct access to data
    const uint8_t* data() const { return data_.data(); }
    uint8_t* data() { return data_.data(); }
    
    // Size and capacity
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void reserve(size_t n) { data_.reserve(n); }
    void clear() { data_.clear(); }

    // Consume bytes from front (efficient enough for vector if not too frequent, 
    // or we could use an offset, but let's keep it simple for now as per request)
    void consume(size_t bytes) {
        if (bytes >= data_.size()) {
            data_.clear();
        } else {
            data_.erase(data_.begin(), data_.begin() + bytes);
        }
    }

    // Expose internal vector if needed
    const container_type& getVector() const { return data_; }
};

} // namespace websrv