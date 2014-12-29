This is one-to-one chat.
There are two versions of it:
    - connection-oriented (server and client are using TCP protocol)
    - connection-less (using UDP protocol)

Architecture is as follows:
    Application <---> Network library (TCP/UDP) <---> Network <--->
                <---> Network library (TCP/UDP) <---> Application

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
class MessageIO {
    /*!
        Callback for network part when it receives any message
     */
    MsgReceived() {
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
        call to Sender method
    }

    /*!
        Callback to use when there is any message to send
     */
    Sender() {
        implementation dependent? // TCP/UDP dependent?
    }
}
---- snip end ----

MessageIO has data like:
    - received message queue,
    - send message queue,
    - link to notification variable to notify application with
