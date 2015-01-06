#include "Parser.hpp"
#include "Message.hpp"
#include <boost/shared_ptr.hpp>

Parser::Parser()
{
    // set initial state
    _Reset();
    // set categories handlers
    m_categoryHandler['m'] = _Cat_m_Handler;
}

void
Parser::_Reset()
{
    m_state = STAND_BY_STATE;
}

Parser::ParserState
Parser::State() const
{
    return m_state;
}

void
Parser::ParseMessage(const MessagePtr &)
{
    // implements state-machine
    Parser::ParserSignal _signal =
            m_parserStateDescriptor[m_state].handler(m_state, _msg);
    Parser::ParserState _next_state =
            m_parserStateMachine[m_state][_signal];

    m_state = _next_state;
}

ParsedMessagePtr
Parser::GetParsed()
{
    if (m_state == STAND_BY_STATE) return m_parsedMessage;

    return ParsedMessagePtr();
}

Parser::ParserSignal
Parser::_StandByStateHandler(ParserState _state, const MessagePtr &_msg)
{
    ParserSignal _result;
    char _cat = _msg->msg.get()[0];
    MessageCategoryHandler _handler;

    /// check for category
    try {
        _handler = m_categoryHandler.at(_cat);
    }
    catch (...) throw;

    m_parsedMessage.reset(new ParsedMessage);
    m_parsedMessage->category = CAT_M;
    m_handler = _handler;
    _result = _handler(_msg, true, m_parsedMessage);
    return _result;
}

Parser::ParserSignal
Parser::_IncompleteMessageStateHandler(ParserState _state, const MessagePtr &_msg)
{
    return m_handler(_msg, false, m_parsedMessage);
}

Parser::ParserSignal
Parser::_Cat_m_Handler(const MessagePtr &_msg,
                       ParsedMessagePtr &_parsed,
                       bool first_time)
{
    /// TODO
    static std::size_t offset = 0;
    static std::size_t bytes_copied_nickname = 0,
                       bytes_copied_message = 0;
    std::size_t bytes_to_copy = 0;
    std::size_t bytes_read = 0;

    if (first_time) {
        bytes_copied_message = bytes_copied_nickname = 0;
        offset = 0x0;
    }

    while (bytes_read < _mgs->length) {
        if (offset == 0x00) {
            // category byte
            if (_msg->msg.get()[0] != 'm') return FAILED_MESSAGE_SIGNAL;
            ++bytes_read;
        } else if (offset < 0x11) {
            // nickname
            bytes_to_copy =
                    (0x10 > (_msg->length - bytes_read)) ? _msg->length - bytes_read :
                                                         0x10;
            memcpy(_parsed->parsed.cat_m.nickname + bytes_copied_nickname,
                   _msg->msg.get() + bytes_read,
                   bytes_to_copy);

            bytes_copied_nickname += bytes_to_copy;
            bytes_read += bytes_to_copy;
        } else {
            // message body
            bytes_to_copy =
                    (0x200 > (_msg->length - bytes_read)) ? _msg->length - bytes_read :
                                                            0x200;

            memcpy(_parsed->parsed.cat_m.message + bytes_copied_message,
                   _msg->msg.get() + bytes_read,
                   bytes_to_copy);

            bytes_copied_message += bytes_to_copy;
            bytes_read += bytes_to_copy;
        }
        offset += bytes_read;
    }

    if (offset < 0x211) return STILL_NOT_PARSED_SIGNAL;
    return FINISH_MESSAGE_SIGNAL;
}
