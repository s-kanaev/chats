include(FindPkgConfig)
pkg_check_modules(sqlite REQUIRED sqlite3)

include_directories(.)

file(GLOB_RECURSE SRC *.c)

add_executable(chats-p2p-mu-server ${SRC})
set_target_properties(chats-p2p-mu-server
                      PROPERTIES COMPILE_FLAGS "${sqlite_CFLAGS}")
target_link_libraries(chats-p2p-mu-server chats-p2p-mu-protocol
                                          chats-p2p-mu-crc
                                          chats-common
                                          chats-network
                                          chats-io-service
                                          chats-thread-pool
                                          chats-timer
                                          ${sqlite_LDFLAGS})
