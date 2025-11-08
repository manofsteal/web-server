#pragma once

#include "websrv/area_allocator.hpp"
#include <array>
#include <memory>
#include <vector>

namespace websrv {

// Memory area sizes based on TODO.md strategy
constexpr size_t EVENT_LOOP_AREA_SIZE = 256 * 1024;    // 256KB per iteration
constexpr size_t CONNECTION_AREA_SIZE = 128 * 1024;     // 128KB per connection  
constexpr size_t FRAME_AREA_SIZE = 64 * 1024;          // 64KB per message
constexpr size_t TEMP_AREA_SIZE = 32 * 1024;           // 32KB for temporary data

constexpr size_t MAX_CONNECTIONS = 1000;               // Maximum concurrent connections
constexpr size_t MAX_CONCURRENT_MESSAGES = 100;        // Maximum concurrent message processing

// Individual area wrapper
class ManagedArea {
private:
    std::unique_ptr<char[]> memory_pool;
    std::unique_ptr<AreaAllocatorBase> allocator;

public:
    // Default constructor for array initialization
    ManagedArea() : memory_pool(nullptr), allocator(nullptr) {}
    
    // Constructor with size and name
    ManagedArea(size_t size, const char* name) 
        : memory_pool(std::make_unique<char[]>(size))
        , allocator(std::make_unique<AreaAllocatorBase>(memory_pool.get(), size, name)) {}

    // Move constructor and assignment for array usage
    ManagedArea(ManagedArea&& other) noexcept 
        : memory_pool(std::move(other.memory_pool))
        , allocator(std::move(other.allocator)) {}
        
    ManagedArea& operator=(ManagedArea&& other) noexcept {
        if (this != &other) {
            memory_pool = std::move(other.memory_pool);
            allocator = std::move(other.allocator);
        }
        return *this;
    }

    // Initialize after default construction
    void initialize(size_t size, const char* name) {
        memory_pool = std::make_unique<char[]>(size);
        allocator = std::make_unique<AreaAllocatorBase>(memory_pool.get(), size, name);
    }

    AreaAllocatorBase* get_allocator() { return allocator.get(); }
    const AreaAllocatorBase* get_allocator() const { return allocator.get(); }
    
    void reset() { 
        if (allocator) {
            allocator->reset(); 
        }
    }
    
    // Statistics
    size_t get_used_size() const { return allocator ? allocator->get_used_size() : 0; }
    size_t get_peak_usage() const { return allocator ? allocator->get_peak_usage() : 0; }
    double get_usage_percentage() const { return allocator ? allocator->get_usage_percentage() : 0.0; }
    const char* get_name() const { return allocator ? allocator->get_name() : "uninitialized"; }
};

// Memory areas for a single poller thread
class PollerMemoryAreas {
private:
    // Core areas
    ManagedArea event_loop_area;
    ManagedArea temp_area;
    
    // Connection-specific areas (pool of areas)
    std::array<ManagedArea, MAX_CONNECTIONS> connection_areas;
    size_t next_connection_area;
    
    // Frame-specific areas (pool of areas)  
    std::array<ManagedArea, MAX_CONCURRENT_MESSAGES> frame_areas;
    size_t next_frame_area;

public:
    PollerMemoryAreas() 
        : event_loop_area(EVENT_LOOP_AREA_SIZE, "event_loop")
        , temp_area(TEMP_AREA_SIZE, "temp")
        , connection_areas{}
        , next_connection_area(0)
        , frame_areas{}
        , next_frame_area(0) {
        
        // Initialize connection areas
        for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
            connection_areas[i].initialize(CONNECTION_AREA_SIZE, "connection");
        }
        
        // Initialize frame areas
        for (size_t i = 0; i < MAX_CONCURRENT_MESSAGES; i++) {
            frame_areas[i].initialize(FRAME_AREA_SIZE, "frame");
        }
    }

    // Event loop area - reset every poller iteration
    AreaAllocatorBase* get_event_loop_area() { 
        return event_loop_area.get_allocator(); 
    }
    
    void reset_event_loop_area() { 
        event_loop_area.reset(); 
    }

    // Temporary area - reset frequently
    AreaAllocatorBase* get_temp_area() { 
        return temp_area.get_allocator(); 
    }
    
    void reset_temp_area() { 
        temp_area.reset(); 
    }

    // Connection area allocation - round-robin allocation
    AreaAllocatorBase* allocate_connection_area() {
        size_t area_index = next_connection_area % MAX_CONNECTIONS;
        next_connection_area++;
        
        connection_areas[area_index].reset(); // Reset before use
        return connection_areas[area_index].get_allocator();
    }
    
    void free_connection_area(AreaAllocatorBase* area) {
        // Find and reset the area (for cleanup, though reset happens on reallocation)
        for (auto& managed_area : connection_areas) {
            if (managed_area.get_allocator() == area) {
                managed_area.reset();
                break;
            }
        }
    }

    // Frame area allocation - round-robin allocation
    AreaAllocatorBase* allocate_frame_area() {
        size_t area_index = next_frame_area % MAX_CONCURRENT_MESSAGES;
        next_frame_area++;
        
        frame_areas[area_index].reset(); // Reset before use
        return frame_areas[area_index].get_allocator();
    }
    
    void free_frame_area(AreaAllocatorBase* area) {
        // Find and reset the area (for cleanup, though reset happens on reallocation)
        for (auto& managed_area : frame_areas) {
            if (managed_area.get_allocator() == area) {
                managed_area.reset();
                break;
            }
        }
    }

    // Statistics and monitoring
    struct AreaStats {
        const char* name;
        size_t used_size;
        size_t peak_usage;
        size_t total_size;
        double usage_percentage;
    };

    void get_all_stats(std::vector<AreaStats>& stats) const {
        stats.clear();
        
        // Event loop area
        const auto* el_area = event_loop_area.get_allocator();
        stats.push_back({
            el_area->get_name(),
            el_area->get_used_size(),
            el_area->get_peak_usage(),
            el_area->get_total_size(),
            el_area->get_usage_percentage()
        });
        
        // Temp area
        const auto* temp = temp_area.get_allocator();
        stats.push_back({
            temp->get_name(),
            temp->get_used_size(),
            temp->get_peak_usage(),
            temp->get_total_size(),
            temp->get_usage_percentage()
        });
        
        // Connection areas (only active ones)
        for (const auto& area : connection_areas) {
            const auto* allocator = area.get_allocator();
            if (allocator->get_used_size() > 0) {
                stats.push_back({
                    allocator->get_name(),
                    allocator->get_used_size(),
                    allocator->get_peak_usage(),
                    allocator->get_total_size(),
                    allocator->get_usage_percentage()
                });
            }
        }
        
        // Frame areas (only active ones)
        for (const auto& area : frame_areas) {
            const auto* allocator = area.get_allocator();
            if (allocator->get_used_size() > 0) {
                stats.push_back({
                    allocator->get_name(),
                    allocator->get_used_size(),
                    allocator->get_peak_usage(),
                    allocator->get_total_size(),
                    allocator->get_usage_percentage()
                });
            }
        }
    }

    // Total memory usage across all areas
    size_t get_total_used_memory() const {
        size_t total = 0;
        total += event_loop_area.get_used_size();
        total += temp_area.get_used_size();
        
        for (const auto& area : connection_areas) {
            total += area.get_used_size();
        }
        
        for (const auto& area : frame_areas) {
            total += area.get_used_size();
        }
        
        return total;
    }

    size_t get_total_allocated_memory() const {
        return EVENT_LOOP_AREA_SIZE + TEMP_AREA_SIZE + 
               (CONNECTION_AREA_SIZE * MAX_CONNECTIONS) +
               (FRAME_AREA_SIZE * MAX_CONCURRENT_MESSAGES);
    }
};

// Global poller memory areas (one per thread)
extern thread_local std::unique_ptr<PollerMemoryAreas> poller_memory_areas;

// Initialize memory areas for current thread
void init_poller_memory();

// Cleanup memory areas for current thread  
void cleanup_poller_memory();

// Get current thread's memory areas
PollerMemoryAreas* get_poller_memory_areas();

} // namespace websrv