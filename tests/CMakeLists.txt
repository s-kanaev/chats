
add_executable(tp-test tp.c)
target_link_libraries(tp-test thread-pool)

add_executable(io-test io.c)
target_link_libraries(io-test io-service thread-pool)