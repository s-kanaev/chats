#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

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
     * send a message to the only connected client
     */
    void SendMessage(std::string _msg);

    /*
     * receive data (if ready)
     * return true if data was available
     */
    bool GetMessage(std::string &_msg);

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
    // flags whether some data is received and no GetMessage call hapenned
    bool m_data_ready;

    // io service to use
    boost::shared_ptr<boost::asio::io_service> m_io_service;
    // condition varaible to notify when there is any data ready
    boost::shared_ptr<boost::condition_variable> m_data_ready_notify_cv;
    // flag to show that server has data ready
    bool *m_data_ready_notify_flag;
    // flag mutex
    boost::shared_ptr<boost::mutex> m_data_ready_notify_flag_mutex;

    // endpoint to local
    boost::shared_ptr<boost::asio::ip::tcp::endpoint> m_local;
    // endpoint to remote
    boost::shared_ptr<boost::asio::ip::tcp::endpoint> m_remote;
    // socket to accept connections
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
    // acceptor
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> m_acceptor;

private:
    // no copies of the server allowed
    Server(Server const&);
    Server operator=(Server const&);
};

#endif // SERVER_HPP
