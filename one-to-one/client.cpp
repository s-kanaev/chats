#include "thread-pool.hpp"

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
 *  each client has at most 5 event handlers:
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
