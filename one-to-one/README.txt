This is one-to-one chat.
There are two versions of it:
    - connection-oriented (server and client are using TCP protocol)
    - connection-less (using UDP protocol)

Architecture is as follows:
    Application <---> Network library (TCP/UDP) <---> Network <--->
                <---> Network library (TCP/UDP) <---> Application

Application is: User (app) <---> Parser

============= Network library =============
Regardless of whether TCP or UDP protocol is in use by library application
sees it like a black-box'ed two message queues: incoming and outgoing.
So message is basic building block for both of client and server. Message
is cortege of message length and message itself as an array of bytes:

---- snip -----
typedef struct _Message {
    size_t length;
    char *msg;
} Message;
---- snip end ----

The Message object is an object of message i/o operations with interface like:

---- snip ----
// _MethodName - is used for internals, others - for externals
class MessageIO {
    /*!
        Callback for network part when it receives any message
     */
    _MsgReceived() {
        lock received message queue
        enqueue received message
        unlock the queue
        notify connected receiver (application)
    }

    /*!
        API for application to receive messages on notification
        lets to know whether there are any more messages left
     */
    GetMsg() {
        lock received message queue
        get front message
        unlock queue
        give front message to application
    }

    /*!
        API for application to asynchronously send message
     */
    SendMsg() {
        lock send message queue
        queue message to send
        unlock send message queue
        call to _Sender method
    }

    /*!
        Callback to use when there is any message to send
     */
    _Sender() {
        implementation dependent? // TCP/UDP dependent?
    }

    /*!
     * \brief API to start receiving messages
     * \return if receiving is started -> true, otherwise -> false
     * (default implementation)
     */
    virtual
    bool StartReceiver()
    {
        return _StartReceiver();
    }
}
---- snip end ----

MessageIO has data like:
    - received message queue,
    - send message queue, (no need for this queue, if messsage is sent immidiately)
    - link to notification variable to notify application with

Direct derivatives of MessageIO (like MessageIOTCP) should implement at least _Sender
and _StartReceiver methods. Second-order derivatives of MessageIO (Server, ClientTCP,
ClientUDP) should implement SendMsg and StartReceiver to check for connection state
and should implement connection process. Moreover, neither of the above classes should
know about protocol.

============= Parser =============
Protocol should be implemented in Parser class. Parser class is finite state-machine.
Protocol description:
    // no authentication is provided within protocol
    Message categories:
        - message itself (a text to display)
    Message format:
    byte offset |   size (hex) |    meaning
    0x00            0x01            message category:
                                        'm' = message itself
    0x01            ---             categorized message

Category: Message itself ('m')
    byte offset |   size (hex) |    meaning
    0x01            0x10            nickname to talk with
                                    (zero-end string data, has '\0' padding)
    0x11            0x200           message (zero-end string data)
Total length of 'm'-message is 0x01+0x10+0x200 = 0x211 = 512+16+1 = 529 bytes
Length is fixed for 'm'-category messages

In general Parser has 2 states:
    - incomplete message state (initial)
    - complete message state

'm' category message has following states (in incomplete message state):
    - nickname receive in progress (initial)
    - message receive in progress
    - message received -> complete message state
