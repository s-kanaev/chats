#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstddef>
#include <boost/shared_ptr.hpp>

/// Message object descriptor
typedef struct _Message {
    std::size_t length;
    boost::shared_ptr<char> msg;
} Message;

/// message ibject pointer
typedef boost::shared_ptr<Message> MessagePtr;

#endif // MESSAGE_HPP
