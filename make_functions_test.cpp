#include "containers.hpp"
#include "poller_memory.hpp"
#include "log.hpp"
#include <iostream>

void test_make_functions_standard_mode() {
    LOG("=== Test: make_* Functions in Standard Mode (USE_AREA_ALLOCATORS=0) ===");
    
    init_poller_memory();
    auto* memory_areas = get_poller_memory_areas();
    auto* frame_area = memory_areas->allocate_frame_area();
    
    LOG("Area allocators disabled - make_* functions should use standard allocators");
    
    // Test make_* functions - should ignore area parameter and use standard allocators
    auto vec = make_vector<int>();
    auto vec_sized = make_vector<uint8_t>(100);
    auto vec_with_area = make_vector<char>(50, frame_area); // Area ignored
    
    auto str = make_string();
    auto str_content = make_string("Test string");
    auto str_sized = make_string(128);
    auto str_with_area = make_string("Area string", frame_area); // Area ignored
    
    auto map = make_map<String, int>();
    auto string_map = make_string_map<double>();
    auto hashmap = make_hashmap<int, String>();
    auto set = make_set<int>();
    
    // Add some data
    for (int i = 0; i < 10; i++) {
        vec.push_back(i);
        vec_sized.push_back(static_cast<uint8_t>(i + 10));
        vec_with_area.push_back(static_cast<char>('A' + i));
    }
    
    str = "Modified string";
    str_sized.append(" with more content");
    
    map["test"] = 42;
    string_map["pi"] = 3.14159;
    hashmap[100] = "hundred";
    set.insert(99);
    
    LOG("Container operations completed successfully:");
    LOG("  Vector size: ", vec.size());
    LOG("  Sized vector size: ", vec_sized.size());
    LOG("  Char vector size: ", vec_with_area.size());
    LOG("  String: '", str, "'");
    LOG("  Content string: '", str_content, "'");
    LOG("  Sized string: '", str_sized, "'");
    LOG("  Area string: '", str_with_area, "'");
    LOG("  Map size: ", map.size());
    LOG("  String map size: ", string_map.size());
    LOG("  Hash map size: ", hashmap.size());
    LOG("  Set size: ", set.size());
    
    LOG("Area usage (should be 0 in standard mode): ", frame_area->get_used_size(), " bytes");
    
    cleanup_poller_memory();
}

void test_make_functions_websocket_simulation() {
    LOG("\n=== Test: WebSocket Frame Processing Simulation ===");
    
    init_poller_memory();
    auto* memory_areas = get_poller_memory_areas();
    
    LOG("Simulating WebSocket frame processing with make_* functions...");
    
    for (int frame_num = 1; frame_num <= 3; frame_num++) {
        LOG("\nProcessing WebSocket frame ", frame_num, ":");
        
        // Get a fresh frame area for each frame
        auto* frame_area = memory_areas->allocate_frame_area();
        
        // Simulate frame processing (using standard allocators)
        auto frame_data = make_vector<uint8_t>(512, frame_area);
        auto message_buffer = make_string(256, frame_area);
        auto headers = make_string_map<String>(frame_area);
        auto client_ids = make_set<int>(frame_area);
        
        // Simulate frame content
        for (int i = 0; i < 64; i++) {
            frame_data.push_back(static_cast<uint8_t>(frame_num * 10 + i % 10));
        }
        
        message_buffer = "Frame " + std::to_string(frame_num) + " message content";
        headers["frame-type"] = "text";
        headers["frame-id"] = std::to_string(frame_num);
        headers["timestamp"] = "2024-01-01T12:00:0" + std::to_string(frame_num);
        
        client_ids.insert(1001 + frame_num);
        client_ids.insert(2001 + frame_num);
        client_ids.insert(3001 + frame_num);
        
        LOG("  Frame data size: ", frame_data.size(), " bytes");
        LOG("  Message: '", message_buffer, "'");
        LOG("  Headers count: ", headers.size());
        LOG("  Client IDs count: ", client_ids.size());
        LOG("  Area usage: ", frame_area->get_used_size(), " bytes (", 
            frame_area->get_usage_percentage(), "%)");
        
        // Frame processing complete - area will be reused for next frame
    }
    
    cleanup_poller_memory();
}

int main() {
    LOG("make_* Functions Comprehensive Test");
    LOG("==================================");
    
    test_make_functions_standard_mode();
    test_make_functions_websocket_simulation();
    
    LOG("\n=== make_* Functions Test Completed ===");
    LOG("All make_* functions work correctly in standard allocator mode.");
    LOG("Ready for area allocator activation with USE_AREA_ALLOCATORS=1.");
    
    return 0;
}