cmake_minimum_required(VERSION 3.0)

project(chats C)

include_directories(common)
include_directories(thread-pool)

add_subdirectory(common)
add_subdirectory(thread-pool)
add_subdirectory(tests)