// This test is compiled with USE_AREA_ALLOCATORS=1 to test actual area allocation
#define USE_AREA_ALLOCATORS 1

#include "websrv/containers.hpp"
#include "websrv/poller_memory.hpp"
#include "websrv/log.hpp"
#include <iostream>

int main() {
    LOG("Area Allocator Integration Test (ENABLED)");
    LOG("==========================================");
    
    // Initialize memory areas
    init_poller_memory();
    auto* memory_areas = get_poller_memory_areas();
    auto* frame_area = memory_areas->allocate_frame_area();
    
    LOG("Frame area initial state:");
    LOG("  Used: ", frame_area->get_used_size(), " bytes");
    LOG("  Total: ", frame_area->get_total_size(), " bytes");
    
    // Test area allocator context
    {
        WITH_AREA_ALLOCATOR(frame_area);
        
        LOG("\nCreating area-allocated containers...");
        
        // These SHOULD use area allocators since USE_AREA_ALLOCATORS=1
        auto vec = make_vector<int>();
        auto str = make_string("Test string allocated from area memory");
        auto map = make_map<String, int>();
        
        LOG("After creating empty containers:");
        LOG("  Area used: ", frame_area->get_used_size(), " bytes");
        LOG("  Allocations: ", frame_area->get_allocation_count());
        
        // Add data to containers
        for (int i = 0; i < 50; i++) {
            vec.push_back(i);
        }
        
        map["key1"] = 42;
        map["key2"] = 84;
        map["key3"] = 126;
        
        LOG("\nAfter adding data:");
        LOG("  Vector size: ", vec.size());
        LOG("  String: '", str, "'");
        LOG("  Map size: ", map.size());
        LOG("  Area used: ", frame_area->get_used_size(), " bytes");
        LOG("  Allocations: ", frame_area->get_allocation_count());
        
        // Test more allocations
        auto str2 = make_string("Another string");
        auto vec2 = make_vector<uint8_t>(100); // Pre-sized vector
        
        LOG("\nAfter additional containers:");
        LOG("  Area used: ", frame_area->get_used_size(), " bytes");
        LOG("  Allocations: ", frame_area->get_allocation_count());
        LOG("  Usage: ", frame_area->get_usage_percentage(), "%");
        
    } // Context ends here, memory should still be used
    
    LOG("\nAfter context end:");
    LOG("  Area used: ", frame_area->get_used_size(), " bytes");
    LOG("  Peak usage: ", frame_area->get_peak_usage(), " bytes");
    
    // Reset area to free all memory at once
    frame_area->reset();
    LOG("\nAfter area reset:");
    LOG("  Area used: ", frame_area->get_used_size(), " bytes");
    LOG("  Peak usage: ", frame_area->get_peak_usage(), " bytes");
    
    cleanup_poller_memory();
    
    LOG("\nTest completed - area allocators were ", 
        frame_area->get_peak_usage() > 0 ? "WORKING" : "NOT WORKING");
    
    return 0;
}