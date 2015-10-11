
find_package(Threads)

file(GLOB_RECURSE SRC_LIST *.c)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -rdynamic")

add_library(chats-coroutine SHARED ${SRC_LIST})
target_link_libraries(chats-coroutine chats-common ${CMAKE_THREAD_LIBS_INIT})
