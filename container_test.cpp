#include "containers.hpp"
#include "poller_memory.hpp"
#include "log.hpp"
#include <iostream>

int main() {
    LOG("Container Test: Testing make_* functions");
    
    // Initialize poller memory areas for testing
    init_poller_memory();
    auto* memory_areas = get_poller_memory_areas();
    
    // Test 1: Basic container creation (using standard allocators since USE_AREA_ALLOCATORS=0)
    LOG("Test 1: Basic container creation");
    auto vec = make_vector<int>();
    auto str = make_string("Hello World");
    auto map = make_map<String, int>();
    
    LOG("Vector size: ", vec.size());
    LOG("String content: '", str, "'");
    LOG("Map size: ", map.size());
    
    // Test 2: Pre-sized containers
    LOG("Test 2: Pre-sized containers");
    auto big_vec = make_vector<uint8_t>(1024);
    auto big_str = make_string(256);
    
    LOG("Pre-sized vector capacity: ", big_vec.capacity());
    LOG("Pre-sized string capacity: ", big_str.capacity());
    
    // Test 3: Container creation with explicit area (fallback to standard since disabled)
    LOG("Test 3: Explicit area usage (fallback mode)");
    auto* frame_area = memory_areas->allocate_frame_area();
    auto area_vec = make_vector<uint8_t>(frame_area);
    auto area_str = make_string("Area allocated", frame_area);
    
    LOG("Area vector size: ", area_vec.size());
    LOG("Area string content: '", area_str, "'");
    
    // Test 4: Different container types
    LOG("Test 4: Different container types");
    auto hashmap = make_hashmap<String, int>();
    auto set = make_set<int>();
    auto string_map = make_string_map<int>();
    
    LOG("HashMap size: ", hashmap.size());
    LOG("Set size: ", set.size());
    LOG("StringMap size: ", string_map.size());
    
    // Test 5: Memory area statistics
    LOG("Test 5: Memory area statistics");
    LOG("Frame area used: ", frame_area->get_used_size(), " bytes");
    LOG("Frame area peak: ", frame_area->get_peak_usage(), " bytes");
    LOG("Frame area usage: ", frame_area->get_usage_percentage(), "%");
    
    // Test 6: Container usage patterns
    LOG("Test 6: Container usage simulation");
    
    // Simulate WebSocket frame processing
    for (int i = 0; i < 3; i++) {
        LOG("Processing frame ", i + 1);
        
        // Get fresh frame area
        auto* current_frame_area = memory_areas->allocate_frame_area();
        
        // Create containers for frame processing
        auto frame_data = make_vector<uint8_t>(512, current_frame_area);
        auto message = make_string(128, current_frame_area);
        auto headers = make_string_map<String>(current_frame_area);
        
        // Simulate some data
        frame_data.resize(64);
        message = "Frame message " + std::to_string(i + 1);
        headers["content-type"] = "text/plain";
        
        LOG("  Frame data size: ", frame_data.size());
        LOG("  Message: '", message, "'");
        LOG("  Headers count: ", headers.size());
        LOG("  Area used: ", current_frame_area->get_used_size(), " bytes");
        
        // Area will be reset when next frame allocates it
    }
    
    LOG("Container test completed successfully!");
    
    cleanup_poller_memory();
    return 0;
}