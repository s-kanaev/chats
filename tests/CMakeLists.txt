
add_executable(tp-test tp.c)
target_link_libraries(tp-test chats-thread-pool)

add_executable(io-test io.c)
target_link_libraries(io-test chats-io-service chats-thread-pool chats-timer)