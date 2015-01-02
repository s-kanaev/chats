#ifndef MESSAGEIO_HPP
#define MESSAGEIO_HPP

#include "Message.hpp"
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/weak_ptr.hpp>
#include <queue>
#include <memory>

/*!
 * \brief The MessageIO is message i/o operation interface class
 * General route is like:
 * - user create ServerTCP/UDP or ClientTCP/UDP
 *      neither MessageIOTCP/MessageIOUDP nor a MessageIO directly
 * - connect to/listen for connection
 * - call to StartReceiver() to start receiving messages
 * - call to SendMsg when user wants to send message
 *
 * Derivatives from this class should implement _Sender and _StartReceiver methods
 * and maybe _MsgReceived method
 */
class MessageIO {
public:
    /*!
     * \brief MessageIO constructor
     * \param _app_cv conditional to notify user about message received with
     */
    MessageIO(boost::weak_ptr<boost::condition_variable> _app_cv) :
        m_app_cv(_app_cv)
    {
    }

    /*!
     * \brief API to start receiving messages
     */
    virtual
    void StartReceiver()
    {
        _StartReceiver();
    }

    /*!
     * \brief API for application to receive messages on notification
     * lets to know whether there are any more messages left
     */
    MessagePtr GetMsg() {
        // lock received message queue
        boost::unique_lock<boost::mutex> _l(m_recv_queue_mutex);
        // get front message
        MessagePtr _msg;
        if (!m_recv_queue.empty()) {
            _msg = m_recv_queue.front();
            m_recv_queue.pop();
        }
        // unlock queue
        _l.unlock();
        // give front message to application
        return _msg;
    }

    /*!
     * \brief API for application to asynchronously send message
     */
    virtual
    void SendMsg(MessagePtr _msg) {
        // call to _Sender method
        _Sender(_msg);
    }

protected:
    /********** functions **************/
    /*!
     * \brief callback for network part whenever it receives message
     * actually it is async_receive callback
     */
    virtual
    void _MsgReceived(const boost::system::error_code& e,
                      std::size_t bytes,
                      MessagePtr _msg) {
        // lock received message queue
        boost::unique_lock<boost::mutex> _l(m_recv_queue_mutex);
        // enqueue received message
        m_recv_queue.push(_msg);
        // unlock the queue
        _l.unlock();
        // notify connected receiver (application)
        if (auto _cv = m_app_cv.lock()) {
            _cv->notify_all();
        }
        _StartReceiver();
    }

    /*!
     * \brief actual receiver start function
     */
    virtual
    void _StartReceiver();

    /*!
     * \brief Callback to use when there is any message to send
     * should call to async_send
     */
    virtual
    void _Sender(MessagePtr _msg);

    /*********** variables ***************/
    /// received message queue
    std::queue<MessagePtr> m_recv_queue;

    /// received message queue mutex
    boost::mutex m_recv_queue_mutex;

    /// link to notification variable to notify
    /// application with when some message is received
    boost::weak_ptr<boost::condition_variable> m_app_cv;
private:
};

/// message i/o operation interface class for tcp
/// derivative should implement m_socket setup
class MessageIOTcp : virtual public MessageIO {
public:
    MessageIOTCP(boost::weak_ptr<boost::condition_variable> _app_cv,
                 boost::asio::io_service &_io_service) :
        MessageIO(_app_cv),
        m_socket(_io_service)
    {
    }

protected:
    /*********** functions *************/
    /*!
     * \brief API to start receiving messages
     * create a buffer and call to async_receive
     */
    virtual
    void _StartReceiver()
    {
        boost::asio::socket_base::receive_buffer_size _size;
        m_socket.get_option(_size);
        MessagePtr _msg(new Message);
        _msg->msg.reset(new char[_size.value()]);
        _msg->length = _size.value();
        m_socket.async_receive(boost::asio::buffer(_msg->msg, _msg->length),
                               0, //flags
                               boost::bind(&MessageIOTCP::_MsgReceived,
                                           this,
                                           _1, _2, _msg));
    }

    /*!
     * \brief callback for async_send
     * \param error
     * \param bytes_writen
     * \param _msg
     */
    void _MsgSent(const boost::system::error_code &error,
                std::size_t bytes_writen,
                MessagePtr _msg) {
        /// do smth, especialy on error
    }

    /// will call to async_send of the socket
    void _Sender(MessagePtr _msg) {
        m_socket.async_send(boost::asio::buffer(_msg->msg.get(), _msg->length),
                            0,
                            boost::bind(&MessageIOTcp::OnSend,
                                        this, _1, _2, _msg));
    }

    /*********** variables *************/
    /// socket to send and receive data with
    boost::asio::ip::tcp::socket m_socket;
};

/// message i/o operation class for udp
/// Derivative should put setup for m_socket and m_remote
class MessageIOUDP : virtual public MessageIO {
public:
    MessageIOUDP(boost::weak_ptr<boost::condition_variable> _app_cv,
                 boost::asio::io_service &_io_service) :
        MessageIO(_app_cv),
        m_socket(_io_service)
    {
    }

protected:
    /********** functions ***********/
    /*!
     * \brief API to start receiving messages
     */
    virtual
    void _StartReceiver()
    {
        boost::asio::socket_base::receive_buffer_size _size;
        m_socket.get_option(_size);
        MessagePtr _msg(new Message);
        _msg->msg.reset(new char[_size.value()]);
        _msg->length = _size.value();
        m_socket.async_receive_from(boost::asio::buffer(_msg->msg, _msg->length),
                                    m_remote,
                                    0, //flags
                                    boost::bind(&MessageIOUDP::_MsgReceived,
                                                this,
                                                _1, _2, _msg));
    }

    /*!
     * \brief callback for async_send
     * \param error
     * \param bytes_writen
     * \param _msg
     */
    void _MsgSent(const boost::system::error_code &error,
                std::size_t bytes_writen,
                MessagePtr _msg) {
        /// do smth, especialy on error
    }

    /*!
     * \brief Callback to use when there is any message to send
     * should call to async_send
     */
    virtual
    void _Sender(MessagePtr _msg)
    {
        m_socket.async_send_to(boost::asio::buffer(_msg, _msg->length),
                               m_remote,
                               0, // flags
                               boost::bind(&MessageIOUDP::_MsgSent,
                                           this,
                                           _1, _2, _msg));
    }

    /********** variables ***********/
    /// socket to send and receive data with
    boost::asio::ip::udp::socket m_socket;
    /// remote endpoint to talk with
    boost::asio::ip::udp::endpoint m_remote;
};

#endif // MESSAGEIO_HPP
