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

/// message i/o operation interface class for
class MessageIO {
public:
    /*!
     * \brief callback for network part whenever it receives message
     * actually it is async_receive callback
     */
    virtual
    void MsgReceived(const boost::system::error_code& e,
                     std::size_t bytes,
                     MessagePtr _msg);/* {
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
    }*/

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
    void SendMsg(MessagePtr _msg) {
        // lock send message queue
        boost::unique_lock<boost::mutex> _l(m_send_queue_mutex);
        // queue message to send
        m_send_queue.push(_msg);
        // unlock send message queue
        _l.unlock();
        // call to Sender method (FIXME: asynchronously)
        Sender();
    }

    /*!
     * \brief Callback to use when there is any message to send
     */
    virtual void Sender() {
        // implementation dependent? // TCP/UDP dependent?
    }

protected:
    /// received message queue
    std::queue<MessagePtr> m_recv_queue;
    /// send message queue
    std::queue<MessagePtr> m_send_queue;

    /// received message queue mutex
    boost::mutex m_recv_queue_mutex;
    /// send message queue mutex
    boost::mutex m_send_queue_mutex;

    /// link to notification variable to notify
    /// application with when some message is received
    boost::weak_ptr<boost::condition_variable> m_app_cv;
private:
};

/// message i/o operation interface class for tcp
class MessageIOTcp : public MessageIO {
public:
    virtual void Sender() {

    }

protected:
    /// socket to send and receive data with
    boost::asio::ip::tcp::socket m_socket;
};


#endif // MESSAGEIO_HPP
