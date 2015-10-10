
file(GLOB_RECURSE SRC_LIST *.c)

add_library(io-service STATIC ${SRC_LIST})
target_link_libraries(io-service common)
