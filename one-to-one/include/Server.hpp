#ifndef SERVER_HPP
#define SERVER_HPP

#include "Message.hpp"
#include "MessageIO.hpp"

class ServerTCP : virtual public MessageIOTcp {
};

class ServerUDP : virtual public MessageIOUDP {
};

#endif // SERVER_HPP
