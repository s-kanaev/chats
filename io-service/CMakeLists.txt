
file(GLOB_RECURSE SRC_LIST *.c)

# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -rdynamic")

add_library(io-service SHARED ${SRC_LIST})
target_link_libraries(io-service common)
