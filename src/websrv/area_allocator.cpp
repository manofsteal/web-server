#include "websrv/area_allocator.hpp"

namespace websrv {

// Thread-local storage for current allocator context
thread_local AreaAllocatorBase* current_area_allocator = nullptr;

} // namespace websrv