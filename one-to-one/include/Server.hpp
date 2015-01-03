#ifndef SERVER_HPP
#define SERVER_HPP

#include "Message.hpp"
#include "MessageIO.hpp"

#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/asio.hpp>

/*!
 * \brief The Server class is one-to-one tcp implementation of server
 * route:
 *  - listen to connections
 *  - authorize (exchange data) (?)
 *  - either accept (just continue to use)
 *       or decline next connection (call to Disconnect)
 *  - do data exchange
 */
class Server : virtual public MessageIOTCP {
public:
    /*!
     * \brief Server constructor
     * \param _app_cv conditional to notify user about received message with
     * \param _io_service boost::asio::io_service to use
     * \param _connection_cv  conditional to notify user about next connection coming with
     */
    Server(boost::weak_ptr<boost::condition_variable> _app_cv,
           boost::shared_ptr<boost::asio::io_service> &_io_service,
           boost::weak_ptr<boost::condition_variable> _connection_cv);

    /*!
     * \brief used to listen for next connection
     * \param _port port to listen on
     * notifies user with m_connection_cv
     */
    void Listen(unsigned short _port);
    /*!
     * \brief used to decline connection (shutdown and close the socket)
     */
    void Disconnect();

    /*!
     * \brief API for application to asynchronously send message
     * (should check if server is connected)
     */
    void SendMsg(MessagePtr _msg);
protected:
    /*!
     * \brief callback for async_connect
     * \param err
     */
    void _OnConnection(const boost::system::error_code &err);

    /// conditional to notify user about next connection with
    boost::weak_ptr<boost::condition_variable> m_connection_cv;
    /// endpoint to listen connections on
    boost::asio::ip::tcp::endpoint m_listen_ep;
    /// acceptor to accept connection
    boost::asio::ip::tcp::acceptor m_connection_acceptor;
    /// flags whether the server is in connected state (false by default)
    bool m_connected = false;
};

#endif // SERVER_HPP
