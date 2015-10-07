
file(GLOB_RECURSE SRC_LIST *.c)
add_executable(tp-test ${SRC_LIST})
target_link_libraries(tp-test thread-pool)
