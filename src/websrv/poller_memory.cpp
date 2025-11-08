#include "websrv/poller_memory.hpp"

namespace websrv {

// Thread-local storage for poller memory areas
thread_local std::unique_ptr<PollerMemoryAreas> poller_memory_areas = nullptr;

void init_poller_memory() {
    if (!poller_memory_areas) {
        poller_memory_areas = std::make_unique<PollerMemoryAreas>();
    }
}

void cleanup_poller_memory() {
    poller_memory_areas.reset();
}

PollerMemoryAreas* get_poller_memory_areas() {
    if (!poller_memory_areas) {
        init_poller_memory();
    }
    return poller_memory_areas.get();
}

} // namespace websrv