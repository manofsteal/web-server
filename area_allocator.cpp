#include "area_allocator.hpp"

// Thread-local storage for current allocator context
thread_local AreaAllocatorBase* current_area_allocator = nullptr;