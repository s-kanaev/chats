#include "Server.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

Server::Server(boost::shared_ptr<boost::asio::io_service> _io_service,
               boost::shared_ptr<boost::condition_variable> _message_received_cv,
               boost::shared_ptr<boost::mutex> _flag_mutex,
               bool *_message_received_flag,
               std::size_t _port) :
    m_connected(false),
    m_length_available(0),
    m_io_service(_io_service),
    m_data_ready_notify_cv(_message_received_cv),
    m_data_ready_notify_flag(_message_received_flag),
    m_data_ready_notify_flag_mutex(_flag_mutex)
{
    boost::unique_lock<boost::mutex> lock(*m_data_ready_notify_flag_mutex);
    *m_data_ready_notify_flag = false;
    lock.unlock();

    // create local endpoint (for connection accept)
    m_local.reset(
                new boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
                                                   _port));
    // create socket
    m_remote_socket.reset(
                new boost::asio::ip::tcp::socket(*m_io_service));
    // create acceptor
    m_acceptor.reset(
                new boost::asio::ip::tcp::acceptor(*m_io_service,
                                                   *m_local));
    // start asynchronous accept
    m_acceptor->async_accept(*m_remote_socket,
                             *m_remote,
                             boost::bind(&Server::ConnectionAcceptor,
                                         this,
                                         _1));
}

Server::~Server()
{
}

void Server::ConnectionAcceptor(const boost::system::error_code ec)
{
    // accept a single connection and start async read from socket
    std::cout << "Some one connected to me from: "
              << m_remote->address().to_string()
              << " port: "
              << m_remote->port() << std::endl;
    // FIXME: maybe any authentication procedure should take place?

    m_connected = true;
    // start async read from socket
    m_remote_socket->async_receive(boost::asio::buffer(m_recv_buffer),
                                   0, // FIXME: flags
                                   boost::bind(&Server::RecvMessage,
                                               this,
                                               _1, _2));
}

void Server::RecvMessage(const boost::system::error_code &error, std::size_t bytes_transferred)
{
    boost::unique_lock<boost::mutex> scoped(m_socket_rw_mutex);

    if (error.value() == 0) {
        char *b = m_recv_buffer;
        std::size_t l;

        m_recv_message.clear();
        m_recv_message.push_back(b);

        l = strlen(b);
        m_length_available = l;
        while (l + 1 < bytes_transferred) {
            ++l;
            b += l;
            m_recv_message.push_back(b);
            std::size_t _l = strlen(b);
            l += _l;
            m_length_available += _l;
        }

        // notify external api
        boost::unique_lock<boost::mutex> lock(*m_data_ready_notify_flag_mutex);
        *m_data_ready_notify_flag = true;
        lock.unlock();
        m_data_ready_notify_cv->notify_all();
    }
    // start async read from socket
    m_remote_socket->async_receive(boost::asio::buffer(m_recv_buffer),
                                   0, // FIXME: flags
                                   boost::bind(&Server::RecvMessage,
                                               this,
                                               _1, _2));
}

void Server::SendMessage(std::string _msg)
{
    // send _msg asynchronously
    boost::shared_ptr<boost::unique_lock<boost::mutex> > l(
                new boost::unique_lock<boost::mutex>(m_socket_rw_mutex));

    m_send_buffer.reset(new char[_msg.length()]);
    memcpy(m_send_buffer.get(), _msg.c_str(), _msg.length());
    m_bytes_to_transfer = _msg.length();

    m_remote_socket->async_send(boost::asio::buffer(m_send_buffer.get(), _msg.length()),
                                0, // FIXME: flags
                                boost::bind(&Server::_SendMessage,
                                            this,
                                            l,
                                            _1, _2));
}

void Server::_SendMessage(boost::shared_ptr<boost::unique_lock<boost::mutex> > lock,
                          const boost::system::error_code &error,
                          std::size_t bytes_transferred)
{
    if (error.value() == 0) {
        m_bytes_to_transfer -= bytes_transferred;
        if (m_bytes_to_transfer < 0) m_bytes_to_transfer = 0;

        if (m_bytes_to_transfer == 0) {
            lock->unlock();
            lock.reset();
        }
    } else {
        // some error handling
        lock->unlock();
        lock.reset();
    }
}

bool Server::IsConnected() const
{
    return m_connected;
}

bool Server::GetMessage(std::vector<std::string> &_msg) const
{
    boost::unique_lock<boost::mutex> lock(m_socket_rw_mutex);

    _msg.clear();
    _msg.assign(m_recv_message.begin(), m_recv_message.end());
}

void Server::Shutdown()
{
    m_remote_socket->close();
}
