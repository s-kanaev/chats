
find_package(Threads)

file(GLOB_RECURSE SRC_LIST *.c)

add_library(thread-pool STATIC ${SRC_LIST})
target_link_libraries(thread-pool common ${CMAKE_THREAD_LIBS_INIT})
