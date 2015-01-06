#ifndef PARSER_HPP
#define PARSER_HPP

#include "Message.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <map>

/*!
 * \brief The Parser class
 * Parser class - should parse received messages and create raw messages to send
 */
class Parser {
public:
    /// parser state machine states
    typedef enum _ParserState {
        STAND_BY_STATE = 0, /// message is complete, user can take it out, initial state
        INCOMPLETE_MESSAGE_STATE,   /// message is not complete yet
        PARSER_STATE_MAX
    } ParserState;

    /// constructor
    /// (should set initial state and message categories handlers)
    Parser();

    /// return parser state
    ParserState State() const;

    /// add message to process
    void ParseMessage(MessagePtr _msg);

protected:
    /// parser state-machine signals
    typedef enum _ParserSignal {
        /// signal from STAND_BY_STATE
        NEW_MESSAGE_SIGNAL = 0,     /// new message arrived
        /// signals from INCOMPLETE_MESSAGE_STATE
        STILL_NOT_PARSED_SIGNAL = 0,
        FINISH_MESSAGE_SIGNAL,      /// message parsed
        FAILED_MESSAGE_SIGNAL,      /// failed to parse message
        /// signals count
        PARSER_SIGNAL_MAX = 3
    } ParserSignal;

    /// descriptior of parser state
    typedef struct _ParserStateDescriptor ParserStateDescriptor;
    struct _ParserStateDescriptor {
        /*!
         * global state handler, input = current state,
         * output = signal
         */
        typedef ParserSignal (Parser::*ParserStateHandler)(ParserState);
        ParserStateHandler handler;
    };

    /*!
     * parser state-machine definition
     * used like this:
     * currentSignal = m_parserStateDescriptor[currentState]();
     * next_state = m_parserStateMachine[currentState][currentSignal];
     */
    const
    ParserState m_parserStateMachine[PARSER_STATE_MAX][PARSER_SIGNAL_MAX] =
    {
        /// STAND_BY_STATE
        {
            /// NEW_MESSAGE_SIGNAL
            INCOMPLETE_MESSAGE_STATE
        },
        /// INCOMPLETE_MESSAGE_STATE
        {
            /// STILL_NOT_PARSED_SIGNAL
            INCOMPLETE_MESSAGE_STATE,
            /// FINISH_MESSAGE_SINGAL
            STAND_BY_STATE,
            /// FAILED_MESSAGE_SIGNAL
            STAND_BY_STATE
        }
    };
    const
    ParserStateDescriptor m_ParserStateDescriptor[PARSER_STATE_MAX] =
    {
        /// STAND_BY_STATE
        _StandByStateHandler,
        /// INCOMPLETE_MESSAGE_STATE
        _InclompleteMessageStateHandler
    };

    /*!
     * \brief standby state handler
     * \param _state current state
     * \return signal
     */
    ParserSignal _StandByStateHandler(ParserState _state);

    /*!
     * \brief _InclompleteMessageStateHandler
     * \param _state
     * \return
     */
    ParserSignal _InclompleteMessageStateHandler(ParserState _state);

    /*!
     * \brief handler type for message category
     * input - message to parse
     */
    typedef boost::function<void(const MessagePtr &_msg)> MessageCategoryHandler;

    /*!
     * \brief message category handlers map
     */
    std::map<char, MessageCategoryHandler> CategoryHandler;

    /*!
     * \brief _Cat_m_Handler
     */
    void _Cat_m_Handler();
private:
};

#endif // PARSER_HPP
