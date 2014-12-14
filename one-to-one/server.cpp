#include "thread-pool.hpp"
#include "Server.hpp"
#include "Parser.hpp"

#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>

/*
 * this is one-to-one chat
 * should run like (synchronous):
 *  udp or tcp protocol in use.
 *  single client connects to single client
 *  each client has at most 5 threads:
 *      - one for stdio
 *      - one for network data in
 *      - one for network data out (added when needed)
 *      - one for system signals (sigint, sigterm, etc)
 *      - main thread (control)
 *
 * implementation should be like:
 *  network part (tcp):
 *      two classes: server and client
 *      assume local is server, remote is client
 *      server accepts connection from remote
 *      client connects to remote
 *      both of these two classes talk (i.e. send and receive) messages
 *      received message is passed to parser thru control thread
 *      result of the parser work is then passed to control thread as
 *          a buffer to display
 *      to send a message control thread pass a raw message to parser
 *          and its result is then passed to network out thread to send
 *
 * network part (udp): is much like tcp but with udp, right?
 *
 * here i prefer an asynchronous implementation:
 *  each client (both remote and local) has at most 5 event handlers:
 *      - stdin data ready
 *      - stdout data writen
 *      - network data in ready
 *      - network data out sent
 *      - system signals
 *  so, we have at most 5 threads. each thread is not synchronous
 *  but asynchronous.
 *  moreover, every event handler will have its own thread for
 *  all of them to run simultaneously
 */

#define THREADS_COUNT 5

int main(int argc, char **argv)
{
    boost::shared_ptr<boost::thread_group> _thread_group(new boost::thread_group);
    boost::shared_ptr<boost::asio::io_service> _io_service(new boost::asio::io_service);
    ThreadPool _thread_pool(THREADS_COUNT,
                            _io_service,
                            _thread_group);

    // create async handler for stdin
    // create async handler for stdout
    // create async handler for networkd data in and out (run server)
    // create async handler for system signals

    return 0;
}
