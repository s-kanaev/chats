#include "Client.hpp"
#include "MessageIO.hpp"
#include "Message.hpp"
#include <string>

/************* TCP client class implementation **************/
#undef _ParentClass
#define _ParentClass MessageIOTCP

ClientTCP::ClientTCP(boost::weak_ptr<boost::condition_variable> _app_cv,
                     boost::shared_ptr<boost::asio::io_service> &_io_service,
                     boost::weak_ptr<boost::condition_variable> _connection_cv) :
    MessageIOTCP(_app_cv, _io_service),
    m_connection_cv(_connection_cv),
    m_io_service(_io_service)
{
}

void
ClientTCP::Connect(std::string _addr, unsigned short _port)
{
    if (m_connected) {
        // disconnect then
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        m_socket.close();
    }

    boost::asio::ip::tcp::resolver _r(*m_io_service);
    boost::asio::ip::tcp::resolver::query _q(_addr, std::to_string(_port));
    boost::asio::ip::tcp::resolver::iterator _it = _r.resolve(_q);

    m_remote_ep = *it;

    m_socket.async_connect(m_remote_ep,
                           boost::bind(&ClientTCP::_OnConnect,
                                       this,
                                       _1));
}

void
ClientTCP::_OnConnect(const boost::system::error_code &err)
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
ClientTCP::Disconnect()
{
    if (!m_connected) return;
    m_connected = false;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    m_socket.close();
}

bool
ClientTCP::StartReceiver()
{
    if (!m_connected) return false;
    _ParentClass::StartReceiver();
}

void
ClientTCP::SendMsg(MessagePtr _msg)
{
    if (m_connected)
        _ParentClass::SendMsg(_msg);
}

/************* UDP client class implementation **************/
#undef _ParentClass
#define _ParentClass MessageIOUDP
