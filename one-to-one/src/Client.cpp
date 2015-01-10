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
    MessageIO(_app_cv),
    _ParentClass(_app_cv, _io_service),
    m_connection_cv(_connection_cv),
    m_io_service(_io_service)
{
}

ClientTCP::~ClientTCP()
{
    Disconnect();
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

    m_remote_ep = *_it;

    printf("Connecting to %s : %u\n",
           m_remote_ep.address().to_string().c_str(),
           m_remote_ep.port());
    m_socket.async_connect(m_remote_ep,
                           boost::bind(&ClientTCP::_OnConnect,
                                       this,
                                       _1));
}

void
ClientTCP::_OnConnect(const boost::system::error_code &err)
{
    if (!err) {
        boost::unique_lock<boost::mutex> _scoped(m_connected_mutex);
        m_connected = true;
        _scoped.unlock();
        if (auto _cv = m_connection_cv.lock()) {
            _cv->notify_all();
        }
    }
}

void
ClientTCP::Disconnect()
{
    // block send action
    boost::unique_lock<boost::mutex> _send_lock(m_send_queued_mutex);

    // m_send_queued_mutex is locked by this function, thus there is
    // nothing to send

    boost::unique_lock<boost::mutex> _scoped(m_connected_mutex);

    if (!m_connected) return;
    m_connected = false;

    _scoped.unlock();

    // notify msg recv conditional in case someone waits for it
    if (auto _msg_recv_cv = m_app_cv.lock())
        _msg_recv_cv->notify_all();

    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    m_socket.close();
}

bool
ClientTCP::StartReceiver()
{
    boost::unique_lock<boost::mutex> _scoped(m_connected_mutex);
    if (!m_connected) return false;
    return _ParentClass::StartReceiver();
}

void
ClientTCP::SendMsg(MessagePtr _msg)
{
    boost::unique_lock<boost::mutex> _scoped(m_connected_mutex);
    if (m_connected)
        _ParentClass::SendMsg(_msg);
}

bool
ClientTCP::IsConnected() const
{
    boost::unique_lock<boost::mutex> _scoped(m_connected_mutex);

    return m_connected;
}

/************* UDP client class implementation **************/
#undef _ParentClass
#define _ParentClass MessageIOUDP

ClientUDP::ClientUDP(boost::weak_ptr<boost::condition_variable> _app_cv,
                     boost::shared_ptr<boost::asio::io_service> &_io_service,
                     unsigned short _port) :
    MessageIO(_app_cv),
    _ParentClass(_app_cv, _io_service, _port),
    m_io_service(_io_service)
{
}

void
ClientUDP::_ShutdownSocket()
{
    m_socket.shutdown(boost::asio::ip::udp::socket::shutdown_both);
    m_socket.close();
}

void
ClientUDP::SetRemote(std::string _address, unsigned short _port)
{
    if (m_remote_set) {
        _ShutdownSocket();
    }

    boost::asio::ip::udp::resolver _r(*m_io_service);
    boost::asio::ip::udp::resolver::query _q(_address, std::to_string(_port));
    boost::asio::ip::udp::resolver::iterator _it = _r.resolve(_q);

    m_remote_to_talk = _it->endpoint();

    m_remote_set = true;
}

bool ClientUDP::StartReceiver()
{
    if (!m_remote_set) return false;
    return _ParentClass::StartReceiver();
}

void
ClientUDP::SendMsg(MessagePtr _msg)
{
    if (!m_remote_set) return;
    _ParentClass::SendMsg(_msg);
}

void
ClientUDP::_MsgReceived(const boost::system::error_code &e,
                        std::size_t _bytes,
                        MessagePtr _msg)
{
    if ((m_remote.port() == m_remote_to_talk.port()) &&
        (m_remote.address().to_string() == m_remote_to_talk.address().to_string())) {
        _ParentClass::_MsgReceived(e, _bytes, _msg);
    }
}
