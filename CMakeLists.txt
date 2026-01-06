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

# Common source files - core networking components that compile
set(COMMON_SOURCES
    src/websrv/pollable.cpp
    src/websrv/poller.cpp
    src/websrv/socket.cpp
    src/websrv/listener.cpp
    src/websrv/listener_manager.cpp
    src/websrv/socket_manager.cpp
    src/websrv/network_system.cpp
)

# Common link libraries
set(COMMON_LIBRARIES pthread OpenSSL::SSL OpenSSL::Crypto)

# Working test executables - all use same sources and libraries
add_executable(game_loop_test ${COMMON_SOURCES} test/game_loop_test.cpp)
target_link_libraries(game_loop_test ${COMMON_LIBRARIES})

add_executable(timer_test ${COMMON_SOURCES} test/timer_test.cpp)
target_link_libraries(timer_test ${COMMON_LIBRARIES})

add_executable(socket_error_test ${COMMON_SOURCES} test/socket_error_test.cpp)
target_link_libraries(socket_error_test ${COMMON_LIBRARIES})

add_executable(network_system_test ${COMMON_SOURCES} test/network_system_test.cpp)
target_link_libraries(network_system_test ${COMMON_LIBRARIES})
