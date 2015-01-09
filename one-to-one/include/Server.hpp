#ifndef SERVER_HPP
#define SERVER_HPP

#include "Message.hpp"
#include "MessageIO.hpp"
#include "thread-pool.hpp"

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
     * \param _port port to listen on
     */
    Server(boost::weak_ptr<boost::condition_variable> _app_cv,
           boost::shared_ptr<boost::asio::io_service> &_io_service,
           boost::weak_ptr<boost::condition_variable> _connection_cv,
           boost::shared_ptr<ThreadPool> &_thread_pool,
           unsigned short _port);

    ~Server();

    /*!
     * \brief used to listen for next connection
     * notifies user with m_connection_cv
     */
    void Listen();
    /*!
     * \brief used to decline connection (shutdown and close the socket)
     */
    void Disconnect();

    /*!
     * \brief API for application to asynchronously send message
     * (should check if server is connected)
     */
    void SendMsg(MessagePtr _msg);

    /*!
     * \brief API to start receiving messages
     * \return if receiving is started -> true, otherwise -> false
     */
    bool StartReceiver();

    /*!
     * \brief API to get remote endpoint
     * \return remote endpoint
     */
    boost::asio::ip::tcp::endpoint
    Remote();

    /*!
     * \brief API to check if server is in connected state
     * \return true if connected, false otherwise
     */
    bool
    IsConnected() const;

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
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> m_connection_acceptor;
    /// flags whether the server is in connected state (false by default)
    bool m_connected = false;
    /// m_connected flag mutex
    boost::mutex m_connected_mutex;
    /// pointer to thread pool
    boost::shared_ptr<ThreadPool> m_thread_pool;
    /// port to listen
    unsigned short m_port;
    /// whether server is listening now
    bool m_listening = false;
};

#endif // SERVER_HPP
