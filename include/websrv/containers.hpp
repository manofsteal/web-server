#pragma once

/*
 * Container Type System with Area Allocator Support
 * =================================================
 * 
 * This header provides a unified container type system that can optionally use
 * area allocators for high-performance, allocation-free operation.
 * 
 * Key Features:
 * - Conditional compilation: USE_AREA_ALLOCATORS flag controls allocator usage
 * - Consistent API: Same interface whether using area or standard allocators  
 * - Zero-cost abstraction: No overhead when area allocators are disabled
 * - Thread-local context: Automatic area allocator selection via WITH_AREA_ALLOCATOR()
 * 
 * Usage Examples:
 * 
 * 1. Basic usage (uses thread-local area allocator if enabled):
 *    auto vec = make_vector<int>();
 *    auto str = make_string("hello");
 *    auto map = make_map<String, int>();
 * 
 * 2. Explicit area usage:
 *    auto* area = get_poller_memory_areas()->get_frame_area();
 *    auto vec = make_vector<uint8_t>(area);
 *    auto str = make_string("frame data", area);
 * 
 * 3. Pre-sized containers (avoids reallocations):
 *    auto vec = make_vector<uint8_t>(1024);  // Reserve space for 1024 elements
 *    auto str = make_string(256);            // Reserve space for 256 characters
 * 
 * 4. Context-based allocation:
 *    {
 *        WITH_AREA_ALLOCATOR(frame_area);
 *        auto vec = make_vector<uint8_t>();   // Uses frame_area automatically
 *        auto str = make_string();            // Also uses frame_area
 *    }
 * 
 * 5. WebSocket frame processing example:
 *    void process_websocket_frame(const BufferView& data) {
 *        auto* frame_area = get_poller_memory_areas()->get_frame_area();
 *        WITH_AREA_ALLOCATOR(frame_area);
 *        
 *        auto frame_data = make_vector<uint8_t>(data.size);  // Area-allocated
 *        auto message = make_string();                        // Area-allocated
 *        
 *        // Process frame - all allocations come from frame_area
 *        // Area automatically resets when frame processing is complete
 *    }
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <deque>
#include <array>
#include <queue>
#include <stack>
#include <functional>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>

// Include area allocator support
#include "websrv/area_allocator.hpp"

namespace websrv {

// Configuration flag for area allocator usage
#ifndef USE_AREA_ALLOCATORS
#define USE_AREA_ALLOCATORS 0  // Disabled by default for gradual rollout
#endif

// Container types with conditional area allocator support
#if USE_AREA_ALLOCATORS

// Area-allocated containers
template<typename T>
using Vector = std::vector<T, AreaAllocator<T>>;

template<typename K, typename V>
using Map = std::map<K, V, std::less<K>, AreaAllocator<std::pair<const K, V>>>;

template<typename K, typename V>
using HashMap = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, AreaAllocator<std::pair<const K, V>>>;

template<typename T>
using Set = std::set<T, std::less<T>, AreaAllocator<T>>;

template<typename T>
using HashSet = std::unordered_set<T, std::hash<T>, std::equal_to<T>, AreaAllocator<T>>;

template<typename T>
using List = std::list<T, AreaAllocator<T>>;

template<typename T>
using Deque = std::deque<T, AreaAllocator<T>>;

// String types with area allocators
using String = std::basic_string<char, std::char_traits<char>, AreaAllocator<char>>;

// Note: stringstream with custom allocators is complex, keep standard for now
using StringStream = std::stringstream;
using IStringStream = std::istringstream;
using OStringStream = std::ostringstream;

#else

// Standard STL containers (fallback)
template<typename T>
using Vector = std::vector<T>;

template<typename K, typename V>
using Map = std::map<K, V>;

template<typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template<typename T>
using Set = std::set<T>;

template<typename T>
using HashSet = std::unordered_set<T>;

template<typename T>
using List = std::list<T>;

template<typename T>
using Deque = std::deque<T>;

// String types (standard)
using String = std::string;
using StringStream = std::stringstream;
using IStringStream = std::istringstream;
using OStringStream = std::ostringstream;

#endif

template<typename T, size_t N>
using Array = std::array<T, N>;

template<typename T>
using Queue = std::queue<T>;

template<typename T>
using Stack = std::stack<T>;

// Smart pointers
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Function types
template<typename T>
using Function = std::function<T>;

// I/O types
using IStream = std::istream;
using OStream = std::ostream;
using IOStream = std::iostream;
using IFStream = std::ifstream;
using OFStream = std::ofstream;
using FStream = std::fstream;

// Common type aliases for networking
template<typename T>
using StringMap = Map<String, T>;

// Convenience functions for area-allocated containers
#if USE_AREA_ALLOCATORS

// Create area-allocated vector in current context
// Uses the thread-local area allocator set by WITH_AREA_ALLOCATOR() macro
template<typename T>
Vector<T> make_vector() {
    return Vector<T>(AreaAllocator<T>());
}

// Create area-allocated vector with specific area
// Explicitly uses the provided area allocator instead of thread-local one
template<typename T>
Vector<T> make_vector(AreaAllocatorBase* area) {
    return Vector<T>(AreaAllocator<T>(area));
}

// Create area-allocated vector with initial size
// Pre-allocates space for 'size' elements from current area allocator
template<typename T>
Vector<T> make_vector(size_t size) {
    Vector<T> vec((AreaAllocator<T>()));
    vec.reserve(size);
    return vec;
}

// Create area-allocated vector with initial size and specific area
template<typename T>
Vector<T> make_vector(size_t size, AreaAllocatorBase* area) {
    Vector<T> vec((AreaAllocator<T>(area)));
    vec.reserve(size);
    return vec;
}

// Create area-allocated map in current context
// Uses thread-local area allocator for key-value pair storage
template<typename K, typename V>
Map<K, V> make_map() {
    return Map<K, V>(std::less<K>(), AreaAllocator<std::pair<const K, V>>());
}

// Create area-allocated map with specific area
template<typename K, typename V>
Map<K, V> make_map(AreaAllocatorBase* area) {
    return Map<K, V>(std::less<K>(), AreaAllocator<std::pair<const K, V>>(area));
}

// Create area-allocated hash map in current context
// Uses thread-local area allocator for hash table storage
template<typename K, typename V>
HashMap<K, V> make_hashmap() {
    return HashMap<K, V>(0, std::hash<K>(), std::equal_to<K>(), AreaAllocator<std::pair<const K, V>>());
}

// Create area-allocated hash map with specific area
template<typename K, typename V>
HashMap<K, V> make_hashmap(AreaAllocatorBase* area) {
    return HashMap<K, V>(0, std::hash<K>(), std::equal_to<K>(), AreaAllocator<std::pair<const K, V>>(area));
}

// Create area-allocated hash map with bucket count hint
template<typename K, typename V>
HashMap<K, V> make_hashmap(size_t bucket_count) {
    return HashMap<K, V>(bucket_count, std::hash<K>(), std::equal_to<K>(), AreaAllocator<std::pair<const K, V>>());
}

// Create area-allocated set in current context
template<typename T>
Set<T> make_set() {
    return Set<T>(std::less<T>(), AreaAllocator<T>());
}

// Create area-allocated set with specific area
template<typename T>
Set<T> make_set(AreaAllocatorBase* area) {
    return Set<T>(std::less<T>(), AreaAllocator<T>(area));
}

// Create area-allocated string in current context
// Uses thread-local area allocator for string character storage
inline String make_string() {
    return String("", AreaAllocator<char>());
}

// Create area-allocated string with specific area
inline String make_string(AreaAllocatorBase* area) {
    return String("", AreaAllocator<char>(area));
}

// Create area-allocated string from C string
// Copies the C string content into area-allocated memory
inline String make_string(const char* str) {
    return String(str, AreaAllocator<char>());
}

// Create area-allocated string from C string with specific area
inline String make_string(const char* str, AreaAllocatorBase* area) {
    return String(str, AreaAllocator<char>(area));
}

// Create area-allocated string with reserved capacity
// Pre-allocates space for 'capacity' characters to avoid reallocations
inline String make_string(size_t capacity) {
    String str("", AreaAllocator<char>());
    str.reserve(capacity);
    return str;
}

// Create area-allocated string with reserved capacity and specific area
inline String make_string(size_t capacity, AreaAllocatorBase* area) {
    String str("", AreaAllocator<char>(area));
    str.reserve(capacity);
    return str;
}

// Create area-allocated string map (common usage pattern)
// Map with string keys and template value type
template<typename T>
StringMap<T> make_string_map() {
    return StringMap<T>(std::less<String>(), AreaAllocator<std::pair<const String, T>>());
}

// Create area-allocated string map with specific area
template<typename T>
StringMap<T> make_string_map(AreaAllocatorBase* area) {
    return StringMap<T>(std::less<String>(), AreaAllocator<std::pair<const String, T>>(area));
}

#else

// Fallback functions for standard containers
// These provide the same API but use standard STL allocators

// Standard vector creation - ignores area allocator parameters
template<typename T>
inline Vector<T> make_vector() {
    return Vector<T>();
}

template<typename T>
inline Vector<T> make_vector(AreaAllocatorBase* area) {
    (void)area; // Suppress unused parameter warning
    return Vector<T>();
}

template<typename T>
inline Vector<T> make_vector(size_t size) {
    Vector<T> vec;
    vec.reserve(size);
    return vec;
}

template<typename T>
inline Vector<T> make_vector(size_t size, AreaAllocatorBase* area) {
    (void)area;
    Vector<T> vec;
    vec.reserve(size);
    return vec;
}

// Standard map creation
template<typename K, typename V>
inline Map<K, V> make_map() {
    return Map<K, V>();
}

template<typename K, typename V>
inline Map<K, V> make_map(AreaAllocatorBase* area) {
    (void)area;
    return Map<K, V>();
}

// Standard hash map creation
template<typename K, typename V>
inline HashMap<K, V> make_hashmap() {
    return HashMap<K, V>();
}

template<typename K, typename V>
inline HashMap<K, V> make_hashmap(AreaAllocatorBase* area) {
    (void)area;
    return HashMap<K, V>();
}

template<typename K, typename V>
inline HashMap<K, V> make_hashmap(size_t bucket_count) {
    return HashMap<K, V>(bucket_count);
}

// Standard set creation
template<typename T>
inline Set<T> make_set() {
    return Set<T>();
}

template<typename T>
inline Set<T> make_set(AreaAllocatorBase* area) {
    (void)area;
    return Set<T>();
}

// Standard string creation
inline String make_string() {
    return String();
}

inline String make_string(AreaAllocatorBase* area) {
    (void)area;
    return String();
}

inline String make_string(const char* str) {
    return String(str);
}

inline String make_string(const char* str, AreaAllocatorBase* area) {
    (void)area;
    return String(str);
}

inline String make_string(size_t capacity) {
    String str;
    str.reserve(capacity);
    return str;
}

inline String make_string(size_t capacity, AreaAllocatorBase* area) {
    (void)area;
    String str;
    str.reserve(capacity);
    return str;
}

// Standard string map creation
template<typename T>
inline StringMap<T> make_string_map() {
    return StringMap<T>();
}

template<typename T>
inline StringMap<T> make_string_map(AreaAllocatorBase* area) {
    (void)area;
    return StringMap<T>();
}

#endif

// Make functions for common operations
template<typename T>
inline UniquePtr<T> MakeUnique(T&& value) {
    return std::make_unique<T>(std::forward<T>(value));
}

template<typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
inline SharedPtr<T> MakeShared(T&& value) {
    return std::make_shared<T>(std::forward<T>(value));
}

template<typename T, typename... Args>
inline SharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace websrv