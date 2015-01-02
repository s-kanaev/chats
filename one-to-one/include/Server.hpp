#ifndef SERVER_HPP
#define SERVER_HPP

#include "Message.hpp"
#include "MessageIO.hpp"

#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/asio.hpp>

/*!
 * \brief The ServerTCP class is one-to-one tcp implementation of server
 * route:
 *  - listen to connections
 *  - authorize (exchange data) (?)
 *  - either accept (just continue to use)
 *       or decline next connection (call to Disconnect)
 *  - do data exchange
 */
class Server : virtual public MessageIOTcp {
public:
    /*!
     * \brief ServerTCP constructor
     * \param _app_cv conditional to notify user about received message with
     * \param _io_service boost::asio::io_service to use
     * \param _connection_cv  conditional to notify user about next connection coming with
     */
    ServerTCP(boost::weak_ptr<boost::condition_variable> _app_cv,
              boost::asio::io_service &_io_service,
              boost::weak_ptr<boost::condition_variable> _connection_cv);

    /*!
     * \brief used to listen for next connection
     * notifies user with m_connection_cv
     */
    void Listen();
    /*!
     * \brief used to decline connection
     */
    void Disconnect();
protected:
    /// conditional to notify user about next connection with
    boost::weak_ptr<boost::condition_variable> m_connection_cv;
    /// flags whether the server is in connected state (false by default)
    bool m_connected = false;
};

#endif // SERVER_HPP
