cmake_minimum_required(VERSION 3.10)
project(web_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix for macOS SDK issues - use the correct SDK path
execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE CMAKE_OSX_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add include directories
include_directories(include)

# Core source files (refactored)
set(CORE_SOURCES
    src/websrv/pollable.cpp
    src/websrv/poller.cpp
    src/websrv/socket.cpp
    src/websrv/listener.cpp
    src/websrv/listener_manager.cpp
    src/websrv/socket_manager.cpp
    src/websrv/network_system.cpp
)

# Common source files (without WebSocket components that require OpenSSL)
set(COMMON_SOURCES
    ${CORE_SOURCES}
    src/websrv/http_client.cpp
    src/websrv/http_server.cpp
)

# WebSocket source files (require OpenSSL)
set(WEBSOCKET_SOURCES
    ${COMMON_SOURCES}
    src/websrv/websocket_client.cpp
    src/websrv/websocket_server.cpp
)

# Minimal sources for basic tests
set(MINIMAL_SOURCES
    src/websrv/pollable.cpp
    src/websrv/poller.cpp
    src/websrv/socket.cpp
    src/websrv/listener.cpp
    src/websrv/sequence.cpp
)

# Create socket test
add_executable(socket_test ${MINIMAL_SOURCES} test/socket_test.cpp)
target_link_libraries(socket_test pthread)

# Create socket tests (minimal dependencies)
add_executable(ping_pong_server_test src/websrv/pollable.cpp src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp test/ping_pong_server_test.cpp)
target_link_libraries(ping_pong_server_test pthread)

add_executable(ping_pong_client_test src/websrv/pollable.cpp src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp test/ping_pong_client_test.cpp)
target_link_libraries(ping_pong_client_test pthread)

# Create HTTP client test 2
add_executable(http_client_2_test ${MINIMAL_SOURCES} src/websrv/http_client.cpp test/http_client_2_test.cpp)
target_link_libraries(http_client_2_test pthread)

# Create HTTP server test (now requires WebSocket sources for upgrade support)
add_executable(http_server_test ${WEBSOCKET_SOURCES} test/http_server_test.cpp)
target_link_libraries(http_server_test pthread OpenSSL::SSL OpenSSL::Crypto)

# Create sequence test
add_executable(sequence_test src/websrv/pollable.cpp src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp src/websrv/sequence.cpp test/sequence_test.cpp)
target_link_libraries(sequence_test pthread)

# Create sequence resumable test
add_executable(sequence_resumable_test src/websrv/pollable.cpp src/websrv/poller.cpp src/websrv/socket.cpp src/websrv/listener.cpp src/websrv/sequence.cpp test/sequence_resumable_test.cpp)
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

# Game loop tests
add_executable(game_loop_test ${CORE_SOURCES} test/game_loop_test.cpp)
target_link_libraries(game_loop_test pthread)

add_executable(timer_test ${CORE_SOURCES} test/timer_test.cpp)
target_link_libraries(timer_test pthread)

add_executable(multi_connection_test ${CORE_SOURCES} test/multi_connection_test.cpp)
target_link_libraries(multi_connection_test pthread)

add_executable(socket_error_test ${CORE_SOURCES} test/socket_error_test.cpp)
target_link_libraries(socket_error_test pthread)

add_executable(manager_ops_test ${CORE_SOURCES} test/manager_ops_test.cpp)
target_link_libraries(manager_ops_test pthread)

add_executable(network_system_test ${CORE_SOURCES} test/network_system_test.cpp)
target_link_libraries(network_system_test pthread)
