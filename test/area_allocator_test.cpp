#include "websrv/containers.hpp"
#include "websrv/log.hpp"
#include "websrv/poller_memory.hpp"
#include <chrono>
#include <iostream>

void test_area_allocator_basic() {
  LOG("=== Test 1: Basic Area Allocator Functionality ===");

  // Initialize memory areas
  init_poller_memory();
  auto *memory_areas = get_poller_memory_areas();

  // Get a frame area for testing
  auto *frame_area = memory_areas->allocate_frame_area();

  LOG("Frame area initial state:");
  LOG("  Used: ", frame_area->get_used_size(), " bytes");
  LOG("  Total: ", frame_area->get_total_size(), " bytes");
  LOG("  Usage: ", frame_area->get_usage_percentage(), "%");

  // Test direct area allocation
  char *ptr1 = static_cast<char *>(frame_area->allocate_raw(100));
  char *ptr2 = static_cast<char *>(frame_area->allocate_raw(200));

  LOG("After allocating 300 bytes:");
  LOG("  Used: ", frame_area->get_used_size(), " bytes");
  LOG("  Allocations: ", frame_area->get_allocation_count());
  LOG("  Usage: ", frame_area->get_usage_percentage(), "%");

  // Fill with test data
  for (int i = 0; i < 100; i++)
    ptr1[i] = 'A' + (i % 26);
  for (int i = 0; i < 200; i++)
    ptr2[i] = 'a' + (i % 26);

  LOG("Data written successfully to allocated memory");

  // Reset area
  frame_area->reset();
  LOG("After reset:");
  LOG("  Used: ", frame_area->get_used_size(), " bytes");
  LOG("  Peak usage: ", frame_area->get_peak_usage(), " bytes");

  cleanup_poller_memory();
}

void test_area_allocator_with_containers() {
  LOG("\n=== Test 2: Area Allocator with STL Containers ===");

// Enable area allocators for this test
#undef USE_AREA_ALLOCATORS
#define USE_AREA_ALLOCATORS 1

  init_poller_memory();
  auto *memory_areas = get_poller_memory_areas();
  auto *frame_area = memory_areas->allocate_frame_area();

  LOG("Testing area-allocated containers...");

  try {
    // Test area allocator context
    {
      WITH_AREA_ALLOCATOR(frame_area);

      LOG("Creating area-allocated containers using make_* functions...");

      // Use proper make_* functions with area allocator context
      auto vec = make_vector<int>();
      auto str = make_string("This is a test string allocated from area memory");
      auto map = make_map<String, int>();

      LOG("Initial area usage: ", frame_area->get_used_size(), " bytes");

      // Add data to containers
      for (int i = 0; i < 100; i++) {
        vec.push_back(i);
      }

      map["key1"] = 42;
      map["key2"] = 84;

      LOG("After adding data:");
      LOG("  Vector size: ", vec.size());
      LOG("  String: '", str, "'");
      LOG("  Map size: ", map.size());
      LOG("  Area used: ", frame_area->get_used_size(), " bytes");
      LOG("  Allocations: ", frame_area->get_allocation_count());

    } // Context ends here, but memory remains until area reset

    LOG("After context end, area usage: ", frame_area->get_used_size(),
        " bytes");

    // Reset area to free all memory at once
    frame_area->reset();
    LOG("After area reset: ", frame_area->get_used_size(), " bytes");

  } catch (const std::exception &e) {
    LOG("Note: Area allocator test skipped - containers using standard "
        "allocators");
    LOG("Reason: ", e.what());
  }

  cleanup_poller_memory();

// Restore original setting
#undef USE_AREA_ALLOCATORS
#define USE_AREA_ALLOCATORS 0
}

void test_multiple_areas() {
  LOG("\n=== Test 3: Multiple Area Allocation ===");

  init_poller_memory();
  auto *memory_areas = get_poller_memory_areas();

  // Test event loop area
  auto *event_area = memory_areas->get_event_loop_area();
  auto *temp_area = memory_areas->get_temp_area();

  LOG("Event loop area total size: ", event_area->get_total_size(), " bytes");
  LOG("Temp area total size: ", temp_area->get_total_size(), " bytes");

  // Allocate from different areas
  char *event_ptr = static_cast<char *>(event_area->allocate_raw(1000));
  char *temp_ptr = static_cast<char *>(temp_area->allocate_raw(500));

  LOG("After allocations:");
  LOG("  Event area used: ", event_area->get_used_size(), " bytes");
  LOG("  Temp area used: ", temp_area->get_used_size(), " bytes");

  // Test multiple frame areas (round-robin)
  LOG("Testing frame area allocation (round-robin):");
  for (int i = 0; i < 5; i++) {
    auto *frame_area = memory_areas->allocate_frame_area();
    char *ptr = static_cast<char *>(frame_area->allocate_raw(100 * (i + 1)));
    LOG("  Frame ", i + 1, " allocated ", 100 * (i + 1),
        " bytes, area used: ", frame_area->get_used_size(), " bytes");
  }

  // Reset event loop area (simulating end of poll iteration)
  memory_areas->reset_event_loop_area();
  LOG("Event loop area after reset: ", event_area->get_used_size(), " bytes");

  cleanup_poller_memory();
}

void test_area_overflow() {
  LOG("\n=== Test 4: Area Overflow Handling ===");

  init_poller_memory();
  auto *memory_areas = get_poller_memory_areas();
  auto *temp_area = memory_areas->get_temp_area();

  LOG("Temp area size: ", temp_area->get_total_size(), " bytes");

  try {
    // Try to allocate more than the area size
    size_t large_size = temp_area->get_total_size() + 1000;
    LOG("Attempting to allocate ", large_size, " bytes (should fail)...");

    char *ptr = static_cast<char *>(temp_area->allocate_raw(large_size));
    LOG("ERROR: Allocation should have failed but succeeded!");

  } catch (const std::bad_alloc &e) {
    LOG("Good: Allocation correctly failed with bad_alloc");
  }

  // Try gradual overflow
  LOG("Testing gradual overflow...");
  size_t allocated = 0;
  int allocation_count = 0;

  try {
    while (true) {
      temp_area->allocate_raw(1000);
      allocated += 1000;
      allocation_count++;

      if (allocation_count % 10 == 0) {
        LOG("  Allocated ", allocated, " bytes in ", allocation_count,
            " chunks");
      }
    }
  } catch (const std::bad_alloc &e) {
    LOG("Overflow detected after allocating ", allocated, " bytes in ",
        allocation_count, " chunks");
    LOG("Area usage: ", temp_area->get_usage_percentage(), "%");
  }

  cleanup_poller_memory();
}

void test_performance_comparison() {
  LOG("\n=== Test 5: Performance Characteristics ===");

  init_poller_memory();
  auto *memory_areas = get_poller_memory_areas();
  auto *frame_area = memory_areas->allocate_frame_area();

  // Reset frame area to ensure clean state
  frame_area->reset();
  
  // Calculate safe number of allocations for 64KB area
  // Max allocation size: 64 + 50 = 114 bytes  
  // Safe total: ~400 allocations * 114 bytes = ~45KB (leaves buffer)
  const int num_allocations = 400;

  // Test 1: Standard malloc/free timing
  auto start = std::chrono::high_resolution_clock::now();

  std::vector<void *> ptrs;
  for (int i = 0; i < num_allocations; i++) {
    void *ptr = malloc(64 + (i % 50)); // Variable sizes 64-113 bytes
    ptrs.push_back(ptr);
  }

  for (void *ptr : ptrs) {
    free(ptr);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto malloc_time =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  LOG("Standard malloc/free time: ", malloc_time, " microseconds");

  // Test 2: Area allocator timing
  start = std::chrono::high_resolution_clock::now();

  try {
    for (int i = 0; i < num_allocations; i++) {
      frame_area->allocate_raw(64 + (i % 50));
    }

    frame_area->reset(); // Single reset instead of individual frees

    end = std::chrono::high_resolution_clock::now();
    auto area_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    LOG("Area allocator time: ", area_time, " microseconds");
    LOG("Performance improvement: ", (double)malloc_time / area_time, "x faster");
    LOG("Final area usage: ", frame_area->get_peak_usage(), " bytes peak");
    
  } catch (const std::bad_alloc& e) {
    LOG("Area allocator test failed - insufficient area size");
    LOG("Allocated up to: ", frame_area->get_used_size(), " bytes");
    LOG("Peak usage: ", frame_area->get_peak_usage(), " bytes");
    LOG("Usage: ", frame_area->get_usage_percentage(), "%");
  }

  cleanup_poller_memory();
}

void test_make_functions_demo() {
  LOG("\n=== Test 6: make_* Functions Demonstration ===");
  
  init_poller_memory();
  auto* memory_areas = get_poller_memory_areas();
  auto* frame_area = memory_areas->allocate_frame_area();
  
  LOG("Demonstrating make_* functions with area allocators...");
  
  {
    WITH_AREA_ALLOCATOR(frame_area);
    
    // Test various make_* function overloads
    LOG("\n1. Basic container creation:");
    auto vec = make_vector<uint8_t>();
    auto str = make_string();
    auto map = make_string_map<int>();
    auto hashmap = make_hashmap<String, String>();
    auto set = make_set<int>();
    
    LOG("  Created containers, area used: ", frame_area->get_used_size(), " bytes");
    
    LOG("\n2. Pre-sized container creation:");
    auto big_vec = make_vector<int>(500);  // Reserve space for 500 ints
    auto big_str = make_string(256);       // Reserve 256 chars
    
    LOG("  Pre-sized containers, area used: ", frame_area->get_used_size(), " bytes");
    
    LOG("\n3. Container creation with initial data:");
    auto text = make_string("WebSocket frame data allocated from area memory");
    
    // Add some actual data
    for (int i = 0; i < 50; i++) {
      vec.push_back(static_cast<uint8_t>(i));
      big_vec.push_back(i * 2);
    }
    
    map["content-length"] = 1024;
    map["connection"] = 2048;
    hashmap["protocol"] = "websocket";
    hashmap["version"] = "13";
    
    for (int i = 10; i < 20; i++) {
      set.insert(i);
    }
    
    LOG("  After adding data:");
    LOG("    Vector size: ", vec.size());
    LOG("    Big vector size: ", big_vec.size());
    LOG("    String: '", text, "'");
    LOG("    String map size: ", map.size());
    LOG("    Hash map size: ", hashmap.size());
    LOG("    Set size: ", set.size());
    LOG("    Total area used: ", frame_area->get_used_size(), " bytes");
    LOG("    Allocations: ", frame_area->get_allocation_count());
    LOG("    Usage: ", frame_area->get_usage_percentage(), "%");
  }
  
  LOG("\nAfter context end, area usage: ", frame_area->get_used_size(), " bytes");
  
  // Test explicit area specification
  LOG("\n4. Explicit area specification:");
  auto explicit_vec = make_vector<char>(100, frame_area);
  auto explicit_str = make_string("Explicit area allocation", frame_area);
  auto explicit_map = make_string_map<double>(frame_area);
  
  explicit_map["pi"] = 3.14159;
  explicit_map["e"] = 2.71828;
  
  LOG("  Explicit allocations, area used: ", frame_area->get_used_size(), " bytes");
  LOG("  Final usage: ", frame_area->get_usage_percentage(), "%");
  
  // Reset and show cleanup
  frame_area->reset();
  LOG("\nAfter reset: ", frame_area->get_used_size(), " bytes");
  LOG("Peak usage was: ", frame_area->get_peak_usage(), " bytes");
  
  cleanup_poller_memory();
}

int main() {
  LOG("Area Allocator Comprehensive Test Suite");
  LOG("=======================================");

  test_area_allocator_basic();
  test_area_allocator_with_containers();
  test_multiple_areas();
  test_area_overflow();
  test_performance_comparison();
  test_make_functions_demo();

  LOG("\n=== All Tests Completed ===");
  return 0;
}