
file(GLOB_RECURSE SRC_LIST *.c)
add_library(chats-common SHARED ${SRC_LIST})
