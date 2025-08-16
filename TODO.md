
# WebSocket Server Memory Optimization Strategy

## High-Level Approach: Area-Based Memory Allocation

### Core Concept
The main performance bottleneck is memory allocation/deallocation in the hot path (poller event loop). Every iteration of `while(running)` should use pre-allocated memory from a dedicated area allocator, eliminating malloc/free overhead.

## Memory Allocation Analysis

### Current Allocation Patterns
1. **Per-Event Loop Iteration**: 
   - Frame parsing: `Vector<uint8_t>` copies
   - String operations: HTTP parsing, message handling
   - Temporary objects: headers, payloads, responses

2. **Per-Connection**:
   - `WebSocketConnection` objects 
   - Connection state storage
   - Route handlers

3. **Per-Message** (Critical Hot Path):
   - Frame construction/parsing vectors
   - String-to-vector conversions
   - Buffer allocations for socket I/O

## Proposed Architecture: Area Allocator System

### 1. Memory Areas by Lifetime

```cpp
// Per-thread area allocators
struct PollerMemoryAreas {
    AreaAllocator event_loop_area;    // Reset every iteration
    AreaAllocator connection_area;    // Reset on disconnect  
    AreaAllocator frame_area;         // Reset per message
    AreaAllocator temp_area;          // Reset frequently
};
```

### 2. Area Allocation Strategy

#### **Event Loop Area** (Reset every poller iteration)
- **Size**: 64KB - 256KB per iteration
- **Usage**: All temporary allocations during event processing
- **Reset Point**: End of each `while(running)` iteration
- **Contains**: Frame parsing buffers, temporary strings, HTTP parsing

#### **Connection Area** (Reset on connection close)
- **Size**: 32KB - 128KB per connection
- **Usage**: Connection-specific persistent data
- **Reset Point**: When connection closes
- **Contains**: Connection objects, persistent headers, route state

#### **Frame Area** (Reset per message)
- **Size**: 16KB - 64KB per message
- **Usage**: Message processing temporary data
- **Reset Point**: After message fully processed
- **Contains**: Frame construction, payload processing

#### **Temp Area** (Reset frequently)
- **Size**: 8KB - 32KB
- **Usage**: Very short-lived allocations
- **Reset Point**: Multiple times per iteration
- **Contains**: String parsing, small buffers

### 3. Implementation Strategy

#### Phase 1: Container Abstraction (✅ DONE)
- Wrapped all STL containers in `containers.hpp`
- Ready for custom allocator injection

#### Phase 2: Area Allocator Implementation
```cpp
template<typename T>
class AreaAllocator {
    char* memory_pool;
    size_t pool_size;
    size_t current_offset;
    
public:
    T* allocate(size_t n);
    void deallocate(T* ptr, size_t n) {} // No-op
    void reset() { current_offset = 0; }  // Reset area
};
```

#### Phase 3: Custom Container Integration
```cpp
// In containers.hpp
template<typename T>
using Vector = std::vector<T, AreaAllocator<T>>;

template<typename K, typename V>  
using Map = std::map<K, V, std::less<K>, AreaAllocator<std::pair<const K, V>>>;
```

#### Phase 4: Poller Integration
```cpp
// In poller event loop
while (running) {
    // Reset event loop area at start of iteration
    memory_areas.event_loop_area.reset();
    
    // All allocations in this iteration use area allocator
    poll_events();
    process_websocket_frames();  // Uses frame_area
    handle_connections();        // Uses connection_area
    
    // Area automatically "frees" all memory at end of scope
}
```

## Memory Layout Strategy

### 1. Pre-allocated Pools
```cpp
struct WebSocketServerMemory {
    // Static pools allocated at startup
    char event_loop_pool[256KB];
    char connection_pool[128KB * MAX_CONNECTIONS];  
    char frame_pool[64KB * MAX_CONCURRENT_MESSAGES];
    char temp_pool[32KB];
};
```

### 2. Zero-Copy Optimizations
- **String Views**: Use `string_view` for parsing instead of string copies
- **In-place Frame Parsing**: Parse directly from socket buffer
- **Direct Socket Writing**: Build frames directly into socket buffers
- **Buffer Reuse**: Reuse frame construction buffers

### 3. Object Pooling
```cpp
// Pre-allocated object pools
ObjectPool<WebSocketConnection> connection_pool;
ObjectPool<WebSocketFrame> frame_pool;  
ObjectPool<Buffer> buffer_pool;
```

## Performance Targets

### Current State (Before Optimization)
- **Allocations per message**: ~20-30 malloc/free calls
- **Memory fragmentation**: High due to mixed allocation sizes
- **GC pressure**: Constant allocation/deallocation

### Target State (After Optimization)  
- **Allocations per message**: 0 (all from pre-allocated areas)
- **Memory fragmentation**: Eliminated (linear area allocation)
- **Predictable performance**: No malloc/free in hot path

## Implementation Priority

### High Priority (Critical Path)
1. **Frame processing optimization** (Lines 254, 266, 301 in websocket_server.cpp)
2. **HTTP parsing optimization** (Lines 47, 60-61)
3. **String operations** (All string copies in message handling)

### Medium Priority
1. **Connection management** (Line 37 - new WebSocketConnection)
2. **Route handling** (String map operations)
3. **Buffer management** (Socket I/O buffers)

### Low Priority  
1. **Server startup allocations** (Line 13 - server creation)
2. **Static data** (Constants, lookup tables)

## Measurement Strategy

### Benchmarks
1. **Allocations per second**: Measure malloc/free calls
2. **Memory usage patterns**: Track peak/average memory
3. **Latency distribution**: Message processing time
4. **Throughput**: Messages per second sustained

### Tools
- **Valgrind/Massif**: Memory allocation profiling
- **perf**: CPU profiling and cache analysis  
- **Custom allocator metrics**: Area usage statistics
- **Load testing**: Compare with websocat performance

## Expected Performance Gains

### Throughput Improvement
- **Target**: 2-5x improvement in messages/second
- **Reason**: Eliminated malloc/free overhead in hot path

### Latency Improvement  
- **Target**: 50-80% reduction in tail latencies
- **Reason**: Predictable memory access patterns

### Memory Efficiency
- **Target**: 30-50% reduction in memory usage
- **Reason**: Eliminated fragmentation, better cache locality

## Risk Mitigation

### Memory Safety
- **Bounds checking**: Area allocator overflow detection
- **Memory debugging**: Debug mode with full tracking
- **Gradual rollout**: Feature flags for allocator selection

### Complexity Management
- **Incremental implementation**: Phase-by-phase rollout
- **Fallback mechanism**: Option to use standard allocators
- **Testing**: Extensive stress testing with area allocators

## Success Criteria

1. **Zero malloc/free** in steady-state message processing
2. **Competitive performance** with websocat reference implementation  
3. **Stable memory usage** under sustained load
4. **Maintainable code** with clear allocation patterns

This area-based allocation strategy will transform the WebSocket server from allocation-heavy to allocation-free in the critical path, delivering predictable high performance.

---

# Other TODOs

Add Executer for every poller::task(callback) so user/application can decide which thread they want to run on, 
by default, same thread with poller::poll