#include "Parser.hpp"
#include "Message.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

Parser::Parser()
{
    // set initial state
    _Reset();
    // set categories handlers
    m_categoryHandler['m'] = boost::bind(&Parser::_Cat_m_Handler,
                                         this,
                                         _1, _2, _3);

    m_categoryHandler['c'] = boost::bind(&Parser::_Cat_c_Handler,
                                         this,
                                         _1, _2, _3);
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

bool Parser::ParseMessage(const MessagePtr &_msg)
{
    // implements state-machine
    ParserStateDescriptor::ParserStateHandler _handler =
            m_parserStateDescriptor[m_state].handler;

    Parser::ParserSignal _signal =
            (this->*_handler)(m_state, _msg);
    Parser::ParserState _next_state =
            m_parserStateMachine[m_state][_signal];

    m_state = _next_state;

    if ((m_state == STAND_BY_STATE) &&
        (_signal != FAILED_MESSAGE_SIGNAL)) return true;

    return false;
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
    std::map<char, MessageCategoryHandler>::iterator _found =
            m_categoryHandler.find(_cat);
    if (_found == m_categoryHandler.end()) {
        _Reset();
        return FAILED_MESSAGE_SIGNAL;
    }

    _handler = _found->second;

    m_parsedMessage.reset(new ParsedMessage);

    switch (_found->first) {
    case 'm' :
        m_parsedMessage->category = CAT_M;
        break;
    case 'c' :
        m_parsedMessage->category = CAT_C;
        break;
    }

    m_handler = _handler;
    _result = _handler(_msg, m_parsedMessage, true);
    return _result;
}

Parser::ParserSignal
Parser::_IncompleteMessageStateHandler(ParserState _state, const MessagePtr &_msg)
{
    return m_handler(_msg, m_parsedMessage, false);
}

Parser::ParserSignal
Parser::_Cat_m_Handler(const MessagePtr &_msg,
                       ParsedMessagePtr &_parsed,
                       bool first_time)
{
    static std::size_t offset = 0;
    static std::size_t bytes_copied_nickname = 0,
                       bytes_copied_message = 0;
    std::size_t bytes_to_copy = 0;
    std::size_t bytes_read = 0;

    if (first_time) {
        bytes_copied_message = bytes_copied_nickname = 0;
        offset = 0x0;
    }

    while (bytes_read < _msg->length) {
        if (offset == 0x00) {
            // category byte
            if (_msg->msg.get()[0] != 'm') return FAILED_MESSAGE_SIGNAL;
            else _parsed->category = CAT_M;
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

    // finalize message: barriers for printf
    _parsed->parsed.cat_m.nickname[0x10] = '\0';
    _parsed->parsed.cat_m.message[0x200] = '\0';

    return FINISH_MESSAGE_SIGNAL;
}

Parser::ParserSignal
Parser::_Cat_c_Handler(const MessagePtr &_msg,
                       ParsedMessagePtr &_parsed,
                       bool first_time)
{
    static std::size_t offset = 0;
    std::size_t bytes_read = 0;

    if (first_time) {
        offset = 0x0;
    }

    while (bytes_read < _msg->length) {
        if (offset == 0x00) {
            // category byte
            if (_msg->msg.get()[0] != 'c') return FAILED_MESSAGE_SIGNAL;
            else _parsed->category = CAT_C;
            ++bytes_read;
        } else {
            switch (_msg->msg.get()[1]) {
            case 'd' :
                _parsed->parsed.cat_c.action = 'd';
                break;
            default:
                return FAILED_MESSAGE_SIGNAL;
            }
            ++bytes_read;
        }
        offset += bytes_read;
    }

    if (offset < 0x02) return STILL_NOT_PARSED_SIGNAL;
    return FINISH_MESSAGE_SIGNAL;
}

void
Parser::CreateCat_c_Message(const ParsedMessagePtr &_parsed, MessagePtr &_msg)
{
    if (!_parsed.get() ||
        !_msg.get()) return;

    if (_parsed->category != CAT_C) return;

    _msg->length = 0x02;
    _msg->msg.reset(new char[_msg->length]);
    _msg->msg.get()[0] = 'c';
    _msg->msg.get()[1] = _parsed->parsed.cat_c.action;
}

void
Parser::CreateCat_m_Message(const ParsedMessagePtr &_parsed, MessagePtr &_msg)
{
    if (!_parsed.get() ||
        !_msg.get()) return;

    if (_parsed->category != CAT_M) return;

    _msg->length = 0x211;
    _msg->msg.reset(new char[_msg->length]);
    _msg->msg.get()[0] = 'm';
    memcpy(_msg->msg.get()+0x01, _parsed->parsed.cat_m.nickname, 0x10);
    memcpy(_msg->msg.get()+0x11, _parsed->parsed.cat_m.message, 0x200);
}
