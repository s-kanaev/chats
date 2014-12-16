include_directories(include)
aux_source_directory(src SRCS)

set(NEED_BOOST_LIBS boost_system-mt boost_thread-mt)

add_library(lib ${SRCS})

add_executable(server.bin server.cpp)
add_executable(client.bin client.cpp)

target_link_libraries(server.bin lib rt ${NEED_BOOST_LIBS})
target_link_libraries(client.bin lib rt ${NEED_BOOST_LIBS})
