#include "Server.hpp"

#include <cstddef>
#include <cstdlib>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

Server::Server(boost::shared_ptr<boost::asio::io_service> _io_service,
               boost::shared_ptr<boost::condition_variable> _message_received_cv,
               boost::shared_ptr<boost::mutex> _flag_mutex,
               bool *_message_received_flag,
               std::size_t _port) :
    m_connected(false),
    m_data_ready(false),
    m_io_service(_io_service),
    m_data_ready_notify_cv(_message_received_cv),
    m_data_ready_notify_flag(_message_received_flag),
    m_data_ready_notify_flag_mutex(_flag_mutex)
{
    boost::unique_lock<boost::mutex> lock(m_data_ready_notify_flag_mutex);
    *m_data_ready_notify_flag = false;
    lock.unlock();

    // create local endpoint (for socket creation)
    m_local.reset(
                new boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
                                                   _port));
    // create socket
    m_socket.reset(
                new boost::asio::ip::tcp::socket(*m_io_service));
    // create acceptor
    m_acceptor.reset(
                new boost::asio::ip::tcp::acceptor(*m_io_service,
                                                   *m_local));
}
