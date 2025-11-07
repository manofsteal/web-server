#pragma once
#include <cstdint>
#include <list>
#include <vector>

class Buffer {
private:
  static constexpr size_t BLOCK_SIZE = 100 * 1024;
  std::list<std::vector<char>> blocks_;
  size_t total_size_ = 0;

public:
  // Simple append operation
  void append(const char *data, size_t len) {
    if (len == 0)
      return;

    size_t remaining = len;
    const char *current = data;

    while (remaining > 0) {
      // Get or create current block
      if (blocks_.empty() || blocks_.back().size() >= BLOCK_SIZE) {
        blocks_.emplace_back();
        blocks_.back().reserve(BLOCK_SIZE);
      }

      // Calculate how much we can write to current block
      size_t space_in_block = BLOCK_SIZE - blocks_.back().size();
      size_t to_write = std::min(remaining, space_in_block);

      // Append data to current block
      blocks_.back().insert(blocks_.back().end(), current, current + to_write);

      current += to_write;
      remaining -= to_write;
      total_size_ += to_write;
    }
  }

  // Get byte at specific position
  char getAt(size_t pos) const {
    if (pos >= total_size_) {
      return 0; // Or throw exception if preferred
    }

    size_t block_idx = pos / BLOCK_SIZE;
    auto it = blocks_.begin();
    std::advance(it, block_idx);
    size_t block_pos = pos % BLOCK_SIZE;

    return (*it)[block_pos];
  }

  // Set byte at specific position
  void setAt(size_t pos, char value) {
    if (pos >= total_size_) {
      return; // Or throw exception if preferred
    }

    size_t block_idx = pos / BLOCK_SIZE;
    auto it = blocks_.begin();
    std::advance(it, block_idx);
    size_t block_pos = pos % BLOCK_SIZE;

    (*it)[block_pos] = value;
  }

  // Get total size
  size_t size() const { return total_size_; }

  // Clear all data
  void clear() {
    blocks_.clear();
    total_size_ = 0;
  }

  // Append data from containers with .data() and .size()
  template <typename Container> void append(const Container &container) {
    append(container.data(), container.size());
  }

  // Get number of blocks
  size_t blockCount() const { return blocks_.size(); }

  // Get block size
  static constexpr size_t blockSize() { return BLOCK_SIZE; }
};