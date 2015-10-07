cmake_minimum_required(VERSION 3.0)

project(chats C)

include_directories(common)
include_directories(thread-pool)
include_directories(coroutine)

add_subdirectory(common)
add_subdirectory(thread-pool)
add_subdirectory(coroutine)
add_subdirectory(tests)
