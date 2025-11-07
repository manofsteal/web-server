cmake_minimum_required(VERSION 3.10)
project(web_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add include directories
include_directories(.)

# Common source files (without WebSocket components that require OpenSSL)
set(COMMON_SOURCES
    poller.cpp
    socket.cpp
    listener.cpp
    http_client.cpp
    http_server.cpp
    sequence.cpp
    area_allocator.cpp
    poller_memory.cpp
)

# Minimal sources for area allocator tests (no HTTP/WebSocket to avoid AreaAllocator + std::hash issues)
set(ALLOCATOR_TEST_SOURCES
    poller.cpp
    socket.cpp
    listener.cpp
    sequence.cpp
    area_allocator.cpp
    poller_memory.cpp
)

# WebSocket source files (require OpenSSL)
set(WEBSOCKET_SOURCES
    ${COMMON_SOURCES}
    websocket_client.cpp
    websocket_server.cpp
)

# Create timer example (minimal dependencies)
add_executable(timer_example poller.cpp socket.cpp listener.cpp timer_example.cpp)
target_link_libraries(timer_example pthread)

# Create socket example (use minimal sources to avoid WebSocket dependencies)
add_executable(socket_example ${ALLOCATOR_TEST_SOURCES} socket_example.cpp)
target_link_libraries(socket_example pthread)

# Create socket examples (minimal dependencies)
add_executable(ping_pong_server poller.cpp socket.cpp listener.cpp ping_pong_server.cpp)
target_link_libraries(ping_pong_server pthread)

add_executable(ping_pong_client poller.cpp socket.cpp listener.cpp ping_pong_client.cpp)
target_link_libraries(ping_pong_client pthread)

# Create HTTP client example 2 (use minimal sources to avoid WebSocket dependencies)
add_executable(http_client_example_2 ${ALLOCATOR_TEST_SOURCES} http_client.cpp http_client_example_2.cpp)
target_link_libraries(http_client_example_2 pthread)

# Create HTTP server example (now requires WebSocket sources for upgrade support)
add_executable(http_server_example ${WEBSOCKET_SOURCES} http_server_example.cpp)
target_link_libraries(http_server_example pthread OpenSSL::SSL OpenSSL::Crypto)

# Create sequence example
add_executable(sequence_example poller.cpp socket.cpp listener.cpp sequence.cpp sequence_example.cpp)
target_link_libraries(sequence_example pthread)

# Create sequence resumable example
add_executable(sequence_resumable_example poller.cpp socket.cpp listener.cpp sequence.cpp sequence_resumable_example.cpp)
target_link_libraries(sequence_resumable_example pthread)

# Create WebSocket client example
add_executable(websocket_client_example ${WEBSOCKET_SOURCES} websocket_client_example.cpp)
target_link_libraries(websocket_client_example pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket client stress test (1000 messages)
add_executable(websocket_client_stress_test ${WEBSOCKET_SOURCES} websocket_client_stress_test.cpp)
target_link_libraries(websocket_client_stress_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket client test
add_executable(websocket_test ${WEBSOCKET_SOURCES} websocket_test.cpp)
target_link_libraries(websocket_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket server example
add_executable(websocket_server_example ${WEBSOCKET_SOURCES} websocket_server_example.cpp)
target_link_libraries(websocket_server_example pthread OpenSSL::SSL OpenSSL::Crypto)

# Create unified HTTP + WebSocket server example
add_executable(unified_server_example ${WEBSOCKET_SOURCES} unified_server_example.cpp)
target_link_libraries(unified_server_example pthread OpenSSL::SSL OpenSSL::Crypto)

# Create container test (use minimal sources to avoid WebSocket dependencies)
add_executable(container_test ${ALLOCATOR_TEST_SOURCES} container_test.cpp)
target_link_libraries(container_test pthread)

# Create area allocator test (use minimal sources to avoid WebSocket dependencies)
add_executable(area_allocator_test ${ALLOCATOR_TEST_SOURCES} area_allocator_test.cpp)
target_link_libraries(area_allocator_test pthread)
add_executable(simple_area_test ${ALLOCATOR_TEST_SOURCES} simple_area_test.cpp)
target_link_libraries(simple_area_test pthread)
add_executable(area_allocator_enabled_test ${ALLOCATOR_TEST_SOURCES} area_allocator_enabled_test.cpp)
target_link_libraries(area_allocator_enabled_test pthread)
target_compile_definitions(area_allocator_enabled_test PRIVATE USE_AREA_ALLOCATORS=1)
add_executable(make_functions_test ${ALLOCATOR_TEST_SOURCES} make_functions_test.cpp)
target_link_libraries(make_functions_test pthread)
