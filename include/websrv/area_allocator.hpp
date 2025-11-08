#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace websrv {

// Forward declaration
class AreaAllocatorBase;

// Thread-local storage for current allocator context
extern thread_local AreaAllocatorBase *current_area_allocator;

// Base class for area allocators
class AreaAllocatorBase {
protected:
  char *memory_pool;
  size_t pool_size;
  size_t current_offset;
  size_t peak_usage;
  size_t allocation_count;
  const char *area_name;

public:
  AreaAllocatorBase(char *pool, size_t size, const char *name)
      : memory_pool(pool), pool_size(size), current_offset(0), peak_usage(0),
        allocation_count(0), area_name(name) {}

  // Allocate raw memory
  void *allocate_raw(size_t size,
                     size_t alignment = alignof(std::max_align_t)) {
    // Align the current offset
    size_t aligned_offset = (current_offset + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + size > pool_size) {
      // Area overflow - this is a critical error in our design
      throw std::bad_alloc();
    }

    void *ptr = memory_pool + aligned_offset;
    current_offset = aligned_offset + size;
    allocation_count++;

    if (current_offset > peak_usage) {
      peak_usage = current_offset;
    }

    return ptr;
  }

  // Deallocate - no-op for area allocators
  void deallocate_raw(void *ptr, size_t size) {
    // No-op: area allocators don't deallocate individual objects
    (void)ptr;
    (void)size;
  }

  // Reset the entire area
  void reset() {
    current_offset = 0;
    allocation_count = 0;
    // Keep peak_usage for statistics
  }

  // Get statistics
  size_t get_used_size() const { return current_offset; }
  size_t get_peak_usage() const { return peak_usage; }
  size_t get_total_size() const { return pool_size; }
  size_t get_allocation_count() const { return allocation_count; }
  const char *get_name() const { return area_name; }
  double get_usage_percentage() const {
    return pool_size > 0 ? (double)current_offset / pool_size * 100.0 : 0.0;
  }
};

// STL-compatible allocator template
template <typename T> class AreaAllocator {
public:
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <typename U> struct rebind { using other = AreaAllocator<U>; };

private:
  AreaAllocatorBase *area;

public:
  // Default constructor uses thread-local area
  AreaAllocator() : area(current_area_allocator) {}

  // Explicit constructor with specific area
  explicit AreaAllocator(AreaAllocatorBase *a) : area(a) {}

  // Copy constructor
  template <typename U>
  AreaAllocator(const AreaAllocator<U> &other) : area(other.get_area()) {}

  // Get the underlying area
  AreaAllocatorBase *get_area() const { return area; }

  // Allocate memory for n objects of type T
  pointer allocate(size_type n) {
    if (!area) {
      throw std::bad_alloc(); // No area allocator set
    }

    size_t size = n * sizeof(T);
    void *ptr = area->allocate_raw(size, alignof(T));
    return static_cast<pointer>(ptr);
  }

  // Deallocate memory (no-op for area allocators)
  void deallocate(pointer ptr, size_type n) {
    if (area) {
      area->deallocate_raw(ptr, n * sizeof(T));
    }
  }

  // Construct object
  template <typename U, typename... Args>
  void construct(U *ptr, Args &&... args) {
    new (ptr) U(std::forward<Args>(args)...);
  }

  // Destroy object
  template <typename U> void destroy(U *ptr) { ptr->~U(); }

  // Comparison operators
  template <typename U> bool operator==(const AreaAllocator<U> &other) const {
    return area == other.get_area();
  }

  template <typename U> bool operator!=(const AreaAllocator<U> &other) const {
    return !(*this == other);
  }
};

// RAII helper for setting thread-local area context
class AreaAllocatorContext {
private:
  AreaAllocatorBase *previous_area;

public:
  explicit AreaAllocatorContext(AreaAllocatorBase *area)
      : previous_area(current_area_allocator) {
    current_area_allocator = area;
  }

  ~AreaAllocatorContext() { current_area_allocator = previous_area; }

  // Non-copyable, non-movable
  AreaAllocatorContext(const AreaAllocatorContext &) = delete;
  AreaAllocatorContext &operator=(const AreaAllocatorContext &) = delete;
  AreaAllocatorContext(AreaAllocatorContext &&) = delete;
  AreaAllocatorContext &operator=(AreaAllocatorContext &&) = delete;
};

// Macro for convenient area context switching
#define WITH_AREA_ALLOCATOR(area) AreaAllocatorContext _ctx(area)

} // namespace websrv