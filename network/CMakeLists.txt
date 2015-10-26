
file(GLOB_RECURSE SRC_LIST *.c)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -rdynamic")

add_library(chats-network SHARED ${SRC_LIST})
target_link_libraries(chats-network chats-common chats-io-service)
