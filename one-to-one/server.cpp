#include "Server.hpp"
#include "Parser.hpp"
#include "thread-pool.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <cstdio>

int main(int argc, char **argv)
{
    unsigned short _port;

    if (argc < 2) {
        printf("usage: %s port\n", argv[0]);
        return 0;
    }

    sscanf(argv[1], "%u", &_port);

    if (_port == 0) {
        fprintf(stderr, "Wrong port number\n");
        return 1;
    }
    printf("port - %u\n", _port);

    boost::shared_ptr<boost::asio::io_service> _io_service(
            new boost::asio::io_service());
    boost::shared_ptr<ThreadPool> _thread_pool(
            new ThreadPool(10, _io_service));
    boost::shared_ptr<boost::condition_variable>
            _connection_cv(new boost::condition_variable),
            _msg_cv(new boost::condition_variable); // msg recv cv, connection cv

    boost::shared_ptr<Server> _server(
            new Server(_msg_cv,
                       _io_service,
                       _connection_cv,
                       _thread_pool,
                       _port)); // server instance
    boost::mutex _m;    // mutex for conditionals

    _server->Listen();

    boost::unique_lock<boost::mutex> _l(_m);
    _connection_cv->wait(_l);
    _l.unlock();

    // so, we've got a connection
    printf("Someone connected.\n");

    return 0;
}
