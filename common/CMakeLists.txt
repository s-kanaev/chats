
file(GLOB_RECURSE SRC_LIST *.c)
add_library(common STATIC ${SRC_LIST})
