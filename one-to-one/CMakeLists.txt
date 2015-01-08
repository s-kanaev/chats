include_directories(include)
aux_source_directory(src SRCS)

set(NEED_BOOST_LIBS boost_system-mt boost_thread-mt)

add_library(lib STATIC ${SRCS})

add_executable(server.bin server.cpp)
add_executable(client-tcp.bin client-tcp.cpp)
add_executable(client-udp.bin client-udp.cpp)

target_link_libraries(server.bin lib pthread rt ${NEED_BOOST_LIBS})
target_link_libraries(client-tcp.bin lib pthread rt ${NEED_BOOST_LIBS})
target_link_libraries(client-udp.bin lib pthread rt ${NEED_BOOST_LIBS})
