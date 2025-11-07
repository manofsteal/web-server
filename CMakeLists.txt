cmake_minimum_required(VERSION 3.10)
project(web_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add include directories
include_directories(include)

# Common source files (without WebSocket components that require OpenSSL)
set(COMMON_SOURCES
    src/websrv/poller.cpp
    src/websrv/socket.cpp
    src/websrv/listener.cpp
    src/websrv/http_client.cpp
    src/websrv/http_server.cpp
    src/websrv/sequence.cpp
    src/websrv/area_allocator.cpp
    src/websrv/poller_memory.cpp
)

# Minimal sources for area allocator tests (no HTTP/WebSocket to avoid AreaAllocator + std::hash issues)
set(ALLOCATOR_TEST_SOURCES
    src/websrv/poller.cpp
    src/websrv/socket.cpp
    src/websrv/listener.cpp
    src/websrv/sequence.cpp
    src/websrv/area_allocator.cpp
    src/websrv/poller_memory.cpp
)

# WebSocket source files (require OpenSSL)
set(WEBSOCKET_SOURCES
    ${COMMON_SOURCES}
    src/websrv/websocket_client.cpp
    src/websrv/websocket_server.cpp
)

# Create timer test (minimal dependencies)
add_executable(timer_test src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp test/timer_test.cpp)
target_link_libraries(timer_test pthread)

# Create socket test (use minimal sources to avoid WebSocket dependencies)
add_executable(socket_test ${ALLOCATOR_TEST_SOURCES} test/socket_test.cpp)
target_link_libraries(socket_test pthread)

# Create socket tests (minimal dependencies)
add_executable(ping_pong_server_test src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp test/ping_pong_server_test.cpp)
target_link_libraries(ping_pong_server_test pthread)

add_executable(ping_pong_client_test src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp test/ping_pong_client_test.cpp)
target_link_libraries(ping_pong_client_test pthread)

# Create HTTP client test 2 (use minimal sources to avoid WebSocket dependencies)
add_executable(http_client_2_test ${ALLOCATOR_TEST_SOURCES} src/websrv/http_client.cpp test/http_client_2_test.cpp)
target_link_libraries(http_client_2_test pthread)

# Create HTTP server test (now requires WebSocket sources for upgrade support)
add_executable(http_server_test ${WEBSOCKET_SOURCES} test/http_server_test.cpp)
target_link_libraries(http_server_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create sequence test
add_executable(sequence_test src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp src/websrv/sequence.cpp test/sequence_test.cpp)
target_link_libraries(sequence_test pthread)

# Create sequence resumable test
add_executable(sequence_resumable_test src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp src/websrv/sequence.cpp test/sequence_resumable_test.cpp)
target_link_libraries(sequence_resumable_test pthread)

# Create WebSocket client test
add_executable(websocket_client_test ${WEBSOCKET_SOURCES} test/websocket_client_test.cpp)
target_link_libraries(websocket_client_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket client stress test (1000 messages)
add_executable(websocket_client_stress_test ${WEBSOCKET_SOURCES} test/websocket_client_stress_test.cpp)
target_link_libraries(websocket_client_stress_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket test
add_executable(websocket_test ${WEBSOCKET_SOURCES} test/websocket_test.cpp)
target_link_libraries(websocket_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create WebSocket server test
add_executable(websocket_server_test ${WEBSOCKET_SOURCES} test/websocket_server_test.cpp)
target_link_libraries(websocket_server_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create unified HTTP + WebSocket server test
add_executable(unified_server_test ${WEBSOCKET_SOURCES} test/unified_server_test.cpp)
target_link_libraries(unified_server_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create container test (use minimal sources to avoid WebSocket dependencies)
add_executable(container_test ${ALLOCATOR_TEST_SOURCES} test/container_test.cpp)
target_link_libraries(container_test pthread)

# Create area allocator test (use minimal sources to avoid WebSocket dependencies)
add_executable(area_allocator_test ${ALLOCATOR_TEST_SOURCES} test/area_allocator_test.cpp)
target_link_libraries(area_allocator_test pthread)
add_executable(simple_area_test ${ALLOCATOR_TEST_SOURCES} test/simple_area_test.cpp)
target_link_libraries(simple_area_test pthread)
add_executable(area_allocator_enabled_test ${ALLOCATOR_TEST_SOURCES} test/area_allocator_enabled_test.cpp)
target_link_libraries(area_allocator_enabled_test pthread)
target_compile_definitions(area_allocator_enabled_test PRIVATE USE_AREA_ALLOCATORS=1)
add_executable(make_functions_test ${ALLOCATOR_TEST_SOURCES} test/make_functions_test.cpp)
target_link_libraries(make_functions_test pthread)
