cmake_minimum_required(VERSION 2.8)
project(chats)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g -O0 -Wl,-Bdynamic -fPIC")

include_directories(include)

add_subdirectory(one-to-one)
add_subdirectory(server-oriented)
add_subdirectory(udp)

