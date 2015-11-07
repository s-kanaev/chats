include_directories(protocol)

file(GLOB_RECURSE PROTOCOL_SRC protocol/*.c)
add_library(chats-p2p-mu-protocol SHARED ${PROTOCOL_SRC})

add_subdirectory(server)
add_subdirectory(client)
