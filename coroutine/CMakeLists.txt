
find_package(Threads)

file(GLOB_RECURSE SRC_LIST *.c)

add_library(coroutine STATIC ${SRC_LIST})
target_link_libraries(coroutine common ${CMAKE_THREAD_LIBS_INIT})
