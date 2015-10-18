cmake_minimum_required(VERSION 3.0)

project(chats C)

add_definitions("-g -O0")

include_directories(common)
include_directories(thread-pool)
include_directories(coroutine)
include_directories(io-service)
include_directories(timer)
include_directories(network)

add_subdirectory(common)
add_subdirectory(thread-pool)
add_subdirectory(coroutine)
add_subdirectory(tests)
add_subdirectory(io-service)
add_subdirectory(timer)
add_subdirectory(network)

add_subdirectory(one-to-one)
