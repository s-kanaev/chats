#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#define RECV_BUFFER_SIZE 1024

/*
 * One to one chat server class
 * Should:
 *  - accept a single connection
 *  - do asynchronous send/receive
 */
class Server {
public:
    /*
     * construct a server w/ io_service provided
     * _message_received_cv will be used to inform about new message arrival
     */
    Server(boost::shared_ptr<boost::asio::io_service> _io_service,
           boost::shared_ptr<boost::condition_variable> _message_received_cv,
           boost::shared_ptr<boost::mutex> _flag_mutex,
           bool *_message_received_flag,
           std::size_t _port);
    ~Server();

    /*
     * connection acceptor (single connection)
     * accept a single connection and start async read from socket
     */
    void ConnectionAcceptor(const boost::system::error_code ec);

    /*
     * asynchronously send a message to the only connected client
     */
    void SendMessage(std::string _msg);

    void _SendMessage(boost::shared_ptr<boost::unique_lock<boost::mutex> > lock,
                      const boost::system::error_code& error,
                      std::size_t bytes_transferred);

    /*
     * asynchronously receive a message
     */
    void RecvMessage(const boost::system::error_code& error, std::size_t bytes_transferred);

    /*
     * give data to external api
     * return true if data was available
     * return = !!m_length_available
     */
    bool GetMessage(std::vector<std::string> &_msg) const;

    /*
     * check whether a server has got a connection
     */
    bool IsConnected(void) const;

    /*
     * shutdown the server
     */
    void Shutdown(void);

protected:
    // flags whether the server is connected
    bool m_connected;
    // shows how much data available
    std::size_t m_length_available;

    // io service to use
    boost::shared_ptr<boost::asio::io_service> m_io_service;

    // condition varaible to notify when there is any data ready
    boost::shared_ptr<boost::condition_variable> m_data_ready_notify_cv;
    // flag to show that server has data ready
    bool *m_data_ready_notify_flag;
    // flag mutex
    boost::shared_ptr<boost::mutex> m_data_ready_notify_flag_mutex;

    /*
     * endpoint to local. this endpoint is used by acceptor
     * to accept connection
     */
    boost::shared_ptr<boost::asio::ip::tcp::endpoint> m_local;
    /*
     * endpoint to remote. filled in by acceptor
     */
    boost::shared_ptr<boost::asio::ip::tcp::endpoint> m_remote;
    /*
     * connection to remote socket. filled in by acceptor
     */
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_remote_socket;
    // acceptor
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;

    /*
     * mutex to use on read/write from/to m_remote_socket
     */
    boost::mutex m_socket_rw_mutex;

    /*
     * buffer for received message (used by async_receive)
     */
    char m_recv_buffer[RECV_BUFFER_SIZE];

    /*
     * buffer to send message
     */
    boost::shared_ptr<char> m_send_buffer;
    /*
     * number of bytes to be transferred
     */
    long long int m_bytes_to_transfer;

    /*
     * buffer for received message (for use by external api)
     */
    std::vector<std::string> m_recv_message;

private:
    // no copies of the server allowed
    Server(Server const&);
    Server operator=(Server const&);
};

#endif // SERVER_HPP
