cmake_minimum_required(VERSION 3.10)
project(web_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add include directories
include_directories(.)

# Common source files
set(COMMON_SOURCES
    poller.cpp
    socket.cpp
    listener.cpp
    http_client.cpp
    http_server.cpp
    # websocket_client.cpp
)

# Create timer example (minimal dependencies)
add_executable(timer_example poller.cpp socket.cpp listener.cpp timer_example.cpp)
target_link_libraries(timer_example pthread)

# Create socket example
add_executable(socket_example ${COMMON_SOURCES} socket_example.cpp)
target_link_libraries(socket_example pthread)

# Create socket examples (minimal dependencies)
add_executable(ping_pong_server poller.cpp socket.cpp listener.cpp ping_pong_server.cpp)
target_link_libraries(ping_pong_server pthread)

add_executable(ping_pong_client poller.cpp socket.cpp listener.cpp ping_pong_client.cpp)
target_link_libraries(ping_pong_client pthread)

# Create HTTP client example 2
add_executable(http_client_example_2 ${COMMON_SOURCES} http_client_example_2.cpp)
target_link_libraries(http_client_example_2 pthread)

# Create HTTP server example
add_executable(http_server_example ${COMMON_SOURCES} http_server_example.cpp)
target_link_libraries(http_server_example pthread)

# # Create WebSocket client example
# add_executable(websocket_client_example ${COMMON_SOURCES} websocket_client_example.cpp)
# target_link_libraries(websocket_client_example pthread OpenSSL::SSL OpenSSL::Crypto) 