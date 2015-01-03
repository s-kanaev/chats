#include "Server.hpp"
#include "MessageIO.hpp"
#include "Message.hpp"

#undef _ParentClass
#define _ParentClass MessageIOTCP

Server::Server(boost::weak_ptr<boost::condition_variable> _app_cv,
               boost::shared_ptr<boost::asio::io_service> &_io_service,
               boost::weak_ptr<boost::condition_variable> _connection_cv) :
    _ParentClass(_app_cv, _io_service),
    m_connection_cv(_connection_cv),
    m_connection_acceptor(*_io_service)
{
}

void
Server::Listen(unsigned short _port)
{
    /// setup socket for listening
//    boost::asio::ip::tcp::resolver _r(*m_io_service);
//    boost::asio::ip::tcp::resolver::query _q(_addr, std::to_string(_port));
//    boost::asio::ip::tcp::resolver::iterator _it = _r.resolve(_q);

    if (_port) {
        // initialize listen endpoint and connection acceptor
        m_listen_ep =
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), _port);
        m_connection_acceptor =
                boost::asio::ip::tcp::acceptor(*m_io_service,
                                               m_listen_ep,
                                               true);
    }

    m_connection_acceptor.async_accept(
                m_socket, // new connection will be accepted into this socket
                m_listen_ep, // listening endpoint
                boost::bind(&Server::_OnConnection,
                            this,
                            _1));
}

void
Server::_OnConnection(const boost::system::error_code &err)
{
    /// TODO do smth on error
    if (!err) {
        m_connected = true;
        if (auto _cv = m_connection_cv.lock()) {
            _cv->notify_all();
        }
    }
}

void
Server::Disconnect()
{
    if (!m_connected) return;
    m_connected = false;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    m_socket.close();
}

bool
Server::StartReceiver()
{
    if (!m_connected) return false;
    _ParentClass::StartReceiver();
}

void
Server::SendMsg(MessagePtr _msg)
{
    if (m_connected)
        _ParentClass::SendMsg(_msg);
}
