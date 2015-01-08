#ifndef PARSER_HPP
#define PARSER_HPP

#include "Message.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <map>

/// category 'm' parsed message
typedef struct _Cat_m_MessageStruct {
    char nickname[0x11];
    char message[0x201];
} Cat_m_MessageStruct;

/// category 'c' parsed message
typedef struct _Cat_c_MessageStruct {
    char action;
} Cat_c_MessageStruct;

/// message categories list
typedef enum _MessageCategory {
    CAT_M,
    CAT_C
} MessageCategory;

/// unified structure for parsed message
typedef struct _ParsedMessage {
    MessageCategory category;
    union {
        Cat_m_MessageStruct cat_m;
        Cat_c_MessageStruct cat_c;
    } parsed;
} ParsedMessage;
typedef boost::shared_ptr<ParsedMessage> ParsedMessagePtr;

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

    /// add message to process, return true if parsing finished
    bool ParseMessage(const MessagePtr &_msg);

    /// create category 'm' message
    void CreateCat_m_Message(const ParsedMessagePtr &_parsed,
                             MessagePtr &_msg);

    /// create category 'c' message
    void CreateCat_c_Message(const ParsedMessagePtr &_parsed,
                             MessagePtr &_msg);

    /// get message from parser
    ParsedMessagePtr GetParsed();

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
    typedef struct _ParserStateDescriptor {
        /*!
         * global state handler
         * input = current state, message to parse
         * output = signal
         */
        typedef ParserSignal (Parser::*ParserStateHandler)(ParserState,
                                                           const MessagePtr &);
        ParserStateHandler handler;
    } ParserStateDescriptor;

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
    ParserStateDescriptor m_parserStateDescriptor[PARSER_STATE_MAX] =
    {
        /// STAND_BY_STATE
        {&Parser::_StandByStateHandler},
        /// INCOMPLETE_MESSAGE_STATE
        {&Parser::_IncompleteMessageStateHandler}
    };

    /*!
     * \brief standby state handler
     * \param _state current state
     * \return signal
     */
    ParserSignal _StandByStateHandler(ParserState _state,
                                      const MessagePtr &_msg);

    /*!
     * \brief _InclompleteMessageStateHandler
     * \param _state
     * \return
     */
    ParserSignal _IncompleteMessageStateHandler(ParserState _state,
                                                const MessagePtr &_msg);

    /*!
     * \brief handler type for message category
     * input - message to parse,
     *         message to parsed pointer
     *         flag of first_time start (at standby state)
     */
    typedef boost::function<ParserSignal(const MessagePtr &_msg,
                                         ParsedMessagePtr &_parsed,
                                         bool first_time)>
            MessageCategoryHandler;

    /*!
     * \brief message category handlers map
     */
    std::map<char, MessageCategoryHandler> m_categoryHandler;

    /// current state
    ParserState m_state;
    /// current category handler
    MessageCategoryHandler m_handler;
    /// parsed message pointer
    ParsedMessagePtr m_parsedMessage;

    /*!
     * \brief category 'm' message handler
     * \return signal for Parser state-machine
     */
    ParserSignal _Cat_m_Handler(const MessagePtr &_msg,
                                ParsedMessagePtr &_parsed,
                                bool first_time);

    /*!
     * \brief category 'm' message handler
     * \return signal for Parser state-machine
     */
    ParserSignal _Cat_c_Handler(const MessagePtr &_msg,
                                ParsedMessagePtr &_parsed,
                                bool first_time);

    /// reset state
    void _Reset();
private:
};

#endif // PARSER_HPP
