#ifndef PARSER_HPP
#define PARSER_HPP

#include "Message.hpp"
#include <string>

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
    Parser();

    /// return parser state
    ParserState State() const;

    /// add message to process
    void Parse(MessagePtr _msg);

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
        typedef ParserSignal (Parser::*ParserStateHandler)(ParserState);
        ParserStateHandler handler;
    };

    /// 'm'-message states
    typedef enum _Cat_m_State {
        MESSAGE_RECEIVED_STATE = 0,   /// final state, always = 0 --> COMPLETE_MESSAGE_STATE
        NICKNAME_PROGRESS_STATE,      /// nickname recv in progress. initial state, always = 1
        MESSAGE_PROGRESS_STATE        /// message body recv in progress
    } _Cat_m_State;
    /// 'm'-message signals
    typedef enum _Cat_m_Signal {
        NICKNAME_RECEIVED_SIGNAL,      /// nickname fully received
        MESSAGE_RECEIVED_SIGNAL        /// message body fully received
    } _Cat_m_Signal;

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
    ParserStateDescriptor m_ParserStateDescriptor[PARSER_STATE_MAX];

private:
};

#endif // PARSER_HPP
