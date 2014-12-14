include_directories(include)
aux_source_directory(src SRCS)

add_executable(server.bin server.cpp ${SRCS})
add_executable(client.bin client.cpp ${SRCS})
