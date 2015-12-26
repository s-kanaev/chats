include_directories(protocol)
include_directories(crc)

file(GLOB_RECURSE PROTOCOL_SRC protocol/*.c)
add_library(chats-p2p-mu-protocol STATIC ${PROTOCOL_SRC})

file(GLOB_RECURSE CRC_SRC crc/*.c)
add_library(chats-p2p-mu-crc STATIC ${CRC_SRC})

add_subdirectory(server)
add_subdirectory(client)
