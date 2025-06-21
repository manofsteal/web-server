cmake_minimum_required(VERSION 3.10)
project(web_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(.)

# Common source files
set(COMMON_SOURCES
    poller.cpp
    timer.cpp
    socket.cpp
    listener.cpp
)

# Create timer example
add_executable(timer_example ${COMMON_SOURCES} timer_example.cpp)
target_link_libraries(timer_example pthread)

# Create socket example
add_executable(socket_example ${COMMON_SOURCES} socket_example.cpp)
target_link_libraries(socket_example pthread)

# Create socket examples
add_executable(ping_pong_server ${COMMON_SOURCES} ping_pong_server.cpp)
target_link_libraries(ping_pong_server pthread)

add_executable(ping_pong_client ${COMMON_SOURCES} ping_pong_client.cpp)
target_link_libraries(ping_pong_client pthread) 