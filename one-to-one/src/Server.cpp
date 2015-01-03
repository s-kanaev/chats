#include "Server.hpp"
#include "MessageIO.hpp"
#include "Message.hpp"

Server::Server(boost::weak_ptr<boost::condition_variable> _app_cv,
               boost::shared_ptr<boost::asio::io_service> &_io_service,
               boost::weak_ptr<boost::condition_variable> _connection_cv) :
    MessageIOTCP(_app_cv, _io_service),
    m_connection_cv(_connection_cv)
{
    /// setup socket for listening
}
