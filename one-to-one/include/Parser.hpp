#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>

struct _Message;
typedef struct _Message MessageType;

/*
 * Parser class - should parse received messages and create raw messages to send
 */
class Parser {
public:
    // constructor
    Parser();
    // destructor
    ~Parser();
    // parse raw received message
    MessageType Parse(std::string &_raw);
    // create a raw message to send from message structure
    void ConstructMessage(MessageType &_message, std::string &_raw);
protected:
private:
};

#endif // PARSER_HPP
