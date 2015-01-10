#include "Client.hpp"
#include "Message.hpp"
#include "Parser.hpp"
#include "thread-pool.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// printf mutex
boost::mutex stdio_mutex;

// flag whether recv and send threads should stop (and server too)
bool finish_flag = false;
// mutex for this flag
boost::mutex finish_flag_mutex;

// ctrl-c handler
void Signal_INT_TERM_handler(const boost::system::error_code& error,
                             int signal,
                             boost::shared_ptr<ClientTCP> _client,
                             boost::shared_ptr<boost::condition_variable> _stdin_cv)
{
    // just stop it if no error dispatched
    if (!error) {
        printf("Stopping client\n");

        boost::unique_lock<boost::mutex> _scoped(finish_flag_mutex);
        finish_flag = true;

        _stdin_cv->notify_all();

        Parser _parser;
        ParsedMessagePtr _parsed_msg(new ParsedMessage);
        MessagePtr _msg(new Message);

        _parsed_msg->category = CAT_C;
        _parsed_msg->parsed.cat_c.action = 'd';

        _parser.CreateCat_c_Message(_parsed_msg, _msg);

        _client->SendMsg(_msg);
        _client->Disconnect();
    }
}

void RecvThread(boost::weak_ptr<ClientTCP> _client_ptr,
                boost::shared_ptr<boost::condition_variable> _msg_recv_cv,
                boost::shared_ptr<char> _nickname,
                boost::shared_ptr<boost::condition_variable> _stdin_cv)
{
    Parser _parser;
    MessagePtr _msg;
    boost::unique_lock<boost::mutex> _lock(finish_flag_mutex);
    auto _client = _client_ptr.lock();

    while (!finish_flag) {
        if (!_client.get()) break;

        if (_client->DontHaveMessages())
            _msg_recv_cv->wait(_lock);

        if (finish_flag) break;

        _lock.unlock();

        while (!_client->DontHaveMessages()) {
            _msg = _client->GetMsg();

            if (_msg.get()) {
                if (_parser.ParseMessage(_msg)) {
                    ParsedMessagePtr _parsed_msg;
                    _parsed_msg = _parser.GetParsed();
                    if (_parsed_msg.get()) {
                        boost::unique_lock<boost::mutex> _scoped(stdio_mutex);
                        switch (_parsed_msg->category) {
                        case CAT_M:
                            printf("\n%s > %s\n",
                                   _parsed_msg->parsed.cat_m.nickname,
                                   _parsed_msg->parsed.cat_m.message);
                            // print greeting
                            printf("%s >> ", _nickname.get());
                            fflush(stdout);
                            break;
                        case CAT_C:
                            if (_parsed_msg->parsed.cat_c.action == 'd') {
                                _lock.lock();
                                finish_flag = true;
                                _lock.unlock();
                                _stdin_cv->notify_all();
                            }
                        } // switch (_parsed_msg->category)
                    } // if (_parsed_msg.get())
                } // if (_parser.ParseMessage(_msg))
            } // if (_msg.get())
        }

        _lock.lock();
    }

    printf("%s out!\n", __func__);
}

void StdInThread(char *_buffer,
                 std::size_t _buf_sz,
                 boost::weak_ptr<boost::condition_variable> _cv_ptr)
{
    boost::unique_lock<boost::mutex> _lock(finish_flag_mutex);

    while (!finish_flag) {
        _lock.unlock();

        fgets(_buffer,
              _buf_sz,
              stdin);
        _buffer[_buf_sz - 0x01] = '\0';

        if (auto _cv = _cv_ptr.lock())
            _cv->notify_all();

        _lock.lock();
    }
}

void SendThread(boost::weak_ptr<ClientTCP> _client_ptr,
                boost::shared_ptr<char> _nickname,
                boost::shared_ptr<boost::condition_variable> _stdin_cv)
{
    Parser _parser;
    ParsedMessagePtr _parsed_msg(new ParsedMessage);
    MessagePtr _msg(new Message);
    boost::thread _stdin_thread;

    boost::unique_lock<boost::mutex> _lock(finish_flag_mutex);
    boost::unique_lock<boost::mutex> _stdio_lock(stdio_mutex);

    _stdio_lock.unlock();

    _parsed_msg->category = CAT_M;

    memcpy(_parsed_msg->parsed.cat_m.nickname,
           _nickname.get(),
           strlen(_nickname.get()) > 0x10 ? 0x10 : strlen(_nickname.get()));

    _stdin_thread = boost::thread(boost::bind(StdInThread,
                                              _parsed_msg->parsed.cat_m.message,
                                              0x200,
                                              _stdin_cv));

    while (!finish_flag) {
        _lock.unlock();

        _stdio_lock.lock();
        printf("%s >> ", _nickname.get());
        fflush(stdout);
        _stdio_lock.unlock();

        _lock.lock();
        _stdin_cv->wait(_lock);

        if (finish_flag) {
            _stdin_thread.interrupt();
            // do not join the stdin thread, it's very rude! but it works
            // _stdin_thread.join();
            break;
        }

        _lock.unlock();

        std::size_t _l = strlen(_parsed_msg->parsed.cat_m.message);

        if (_parsed_msg->parsed.cat_m.message[_l-1] == '\n' ||
            _parsed_msg->parsed.cat_m.message[_l-1] == '\r')
            _parsed_msg->parsed.cat_m.message[_l-1] = '\0';

        _parser.CreateCat_m_Message(_parsed_msg, _msg);

        if (auto _client = _client_ptr.lock())
            _client->SendMsg(_msg);

        _lock.lock();
    }

    printf("%s out!\n", __func__);
}

int main(int argc, char **argv)
{
    unsigned short _port;
    char _nickname[0x11];

    if (argc < 4) {
        printf("usage: %s address port nickname (16 char at most)\n", argv[0]);
        return 0;
    }

    sscanf(argv[2], "%u", &_port);

    if (_port == 0) {
        fprintf(stderr, "Wrong port number\n");
        return 1;
    }

    memset(_nickname, 0, 0x11);
    sscanf(argv[3], "%15s", _nickname);
    _nickname[0x10-0x01] = '\0';

    boost::shared_ptr<boost::asio::io_service> _io_service(
            new boost::asio::io_service());
    boost::shared_ptr<ThreadPool> _thread_pool(
            new ThreadPool(4, _io_service)); // 4 = net_io(2) + stdin(1) + signals(1)
    boost::shared_ptr<boost::condition_variable>
            _connection_cv(new boost::condition_variable),
            _msg_cv(new boost::condition_variable); // msg recv cv, connection cv

    boost::shared_ptr<ClientTCP> _client(new ClientTCP(
                                             _msg_cv,
                                             _io_service,
                                             _connection_cv)); // client instance
    boost::mutex _m;    // dumb mutex for conditionals
    boost::shared_ptr<boost::condition_variable> _stdio_cv(new boost::condition_variable);

    boost::asio::signal_set _signals(*_io_service, SIGINT, SIGTERM);
    _signals.async_wait(boost::bind(Signal_INT_TERM_handler,
                                    _1, _2, _client, _stdio_cv));

    _client->Connect(argv[1], _port);

    boost::unique_lock<boost::mutex> _l(_m);
    printf("Wait until connected\n");
    _connection_cv->wait(_l,
                         [&] {
                            return _client->IsConnected();
                         });
    _l.unlock();

    printf("Connected to host\n");

    if (!_client->StartReceiver()) {
        printf("Cannot start receiver\n");
    } else {
        boost::shared_ptr<char> _nickname_ptr(new char[0x10]);
        memcpy(_nickname_ptr.get(), _nickname, 0x10);

        boost::thread_group _thread_group;

        _thread_group.create_thread(boost::bind(RecvThread,
                                                boost::weak_ptr<ClientTCP>(_client),
                                                _msg_cv,
                                                _nickname_ptr,
                                                _stdio_cv));

        _thread_group.create_thread(boost::bind(SendThread,
                                                boost::weak_ptr<ClientTCP>(_client),
                                                _nickname_ptr,
                                                _stdio_cv));
        _thread_group.join_all();
    }

//    _l.lock();
//    printf("waiting for message\n");
//    _msg_cv->wait(_l);
//    printf("Message received\n");
//    MessagePtr _new_msg = _client->GetMsg();
//    printf("\t length: %u\n"
//           "\t body: '%s'\n",
//           _new_msg->length,
//           _new_msg->msg.get());

//    MessagePtr _msg(new Message);
//    _msg->length = strlen("Hey there!\n");
//    _msg->msg.reset(strdup("Hey there!\n"), free);
//    printf("Sending message: l = %u, '%s'\n",
//           _msg->length,
//           _msg->msg.get());
//    _client->SendMsg(_msg);
//    printf("Send ok?\n");

    _io_service->stop();

    return 0;
}
