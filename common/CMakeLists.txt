
file(GLOB_RECURSE SRC_LIST *.c)
add_library(common SHARED ${SRC_LIST})
