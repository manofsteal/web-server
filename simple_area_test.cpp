#include "containers.hpp"
#include "poller_memory.hpp"
#include "log.hpp"
#include <iostream>

int main() {
    LOG("Simple Area Allocator Test");
    LOG("===========================");
    
    // Initialize memory areas
    init_poller_memory();
    auto* memory_areas = get_poller_memory_areas();
    
    // Test basic allocation
    auto* frame_area = memory_areas->allocate_frame_area();
    LOG("Frame area size: ", frame_area->get_total_size(), " bytes");
    
    // Test simple allocation
    char* ptr = static_cast<char*>(frame_area->allocate_raw(100));
    LOG("Allocated 100 bytes, area used: ", frame_area->get_used_size(), " bytes");
    
    // Test container creation with USE_AREA_ALLOCATORS=0 (should use standard allocators)
    auto vec = make_vector<int>();
    auto str = make_string("Test");
    
    LOG("Vector size: ", vec.size());
    LOG("String: '", str, "'");
    
    // Test multiple allocations
    for (int i = 0; i < 5; i++) {
        char* ptr = static_cast<char*>(frame_area->allocate_raw(50));
        LOG("Allocation ", i + 1, ": area used ", frame_area->get_used_size(), " bytes");
    }
    
    LOG("Final usage: ", frame_area->get_usage_percentage(), "%");
    
    cleanup_poller_memory();
    LOG("Test completed successfully!");
    return 0;
}