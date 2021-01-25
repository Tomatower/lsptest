#include "connection.h"
#include "connection_handler.h"
#include "messages.h"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/read.hpp>

#include <iostream>
#include <list>
#include <memory>


#define DEBUG_MESSAGETRAFFIC

ConnectionHandler::ConnectionHandler(uint16_t port) :
port(port)
{
    register_messages();
}

// Has to be explicit, for std::unque_ptr with forward declarations
ConnectionHandler::~ConnectionHandler() {}

void ConnectionHandler::run() {
    boost::asio::io_service io_service;

    std::cout << "Using port " << this->port << "\n";

    boost::asio::ip::tcp::endpoint tcp_endpoint{boost::asio::ip::tcp::v4(), this->port};
    boost::asio::ip::tcp::acceptor tcp_acceptor{io_service, tcp_endpoint};

    tcp_acceptor.listen();
    std::list<std::unique_ptr<Connection>> connections;

    std::function<void(const boost::system::error_code&)> accept_connection;
    accept_connection = [&](const boost::system::error_code&) {
        if (!this->running) return;

        connections.remove_if([](const std::unique_ptr<Connection> &c) {
            return c->is_done();
        });

        /*auto conn = */
        connections.emplace_back(std::make_unique<Connection>(io_service, this));
        auto &conn = connections.back(); // Until C++17 support
        //auto conn = new Connection(io_service, this);

        tcp_acceptor.async_accept(conn->socket(), tcp_endpoint, [&] (const boost::system::error_code& ec) {
            if (ec) {
                std::cout << "Handle Connection error: " << ec.message() << "\n";
                return;
            }
            std::cout << "Connection received from " << tcp_endpoint.address() << ":" << tcp_endpoint.port() << "\n";
            conn->handle();
            accept_connection({});
        });
    };

    accept_connection({});

    io_service.run();
    std::cout << "Done, Bye\n";
}


Connection::Connection(boost::asio::io_service &io_service, ConnectionHandler *handler) :
        io_service(io_service),
        _socket(io_service),
        handler(handler)
{
}


static bool strip_empty(const std::string &str) {
    for (unsigned char c : str) {
        if (c != 0 && !std::isspace(c)) return false;
    }
    return true;
}

static std::pair<std::string, std::string> read_header_line(boost::asio::ip::tcp::socket &socket, boost::asio::streambuf  &streambuffer) {
    if (!socket.is_open()) {
        return {"", "closed"};
    }

    // might throw boost::system_error, has to be handled outside
    size_t cnt = boost::asio::read_until(socket, streambuffer, "\r\n");
    std::string str(cnt + 1, '\0');
    std::istream datastream(&streambuffer);
    datastream.read(&str[0], cnt);

    if (strip_empty(str)) {
        return {"", ""};
    }

    // Separate the header
    size_t sep_pos = str.find(": ");
    if (sep_pos == std::string::npos) {
        return {"", "no sep"};
    }
    size_t end_pos = str.find("\r\n");

    // +2 because of ": " as header separator
    return {str.substr(0, sep_pos), str.substr(sep_pos + 2, end_pos)};
}

struct connection_header {
    size_t content_length = 0;
};

static connection_header read_header(boost::asio::ip::tcp::socket &socket, boost::asio::streambuf  &streambuffer) {
    std::pair<std::string, std::string> field;
    connection_header header;
    do {
        try {
            field = read_header_line(socket, streambuffer);
        }
        catch(const boost::system::system_error &err) {
            if (err.code() == boost::asio::error::eof) {
                // EOF is ok.
                break;
            }
            std::cerr << "Could not read header: socket error " << err.code() << ":" << err.what() << "\n";
            break;
        }

        // EXTEND: List Accepted header options here
        if (field.first.empty() && field.second.empty()) {
            break;

        } else if (field.first.empty() && !field.second.empty()) {
            std::cout << "Something is strange " << field.second << "\n";

        } else if (field.first == "Content-Length") {
            size_t l = std::atoi(field.second.c_str());
            //if (l < 1024 * 1024 * 5)  { // TODO: Try to trust a remote network connection?!?!
            if (l > 0) {
                header.content_length = l;
            }
        } else if (field.first == "Content-Type") {
            if (field.second != "application/vscode-jsonrpc; charset=utf-8") {
                std::cerr << "unexpected content type " << field.second << "\n";
            }

        } else {
            std::cerr << "Unknown header field " << field.first << "\n";
        }
    } while (!field.first.empty());

    return header;
}

void Connection::handle() {
    this->running = true;

    boost::asio::spawn(io_service, [&](boost::asio::yield_context ) {
        // 512K is a nice buffer i think - maybe we have to think about not having it on the stack...
        //const int data_max_size = 1024 * 512;
        //char buffer[data_max_size];
        boost::asio::streambuf streambuffer;

        while (this->_socket.is_open()) {
            connection_header header = read_header(this->_socket, streambuffer);
            if (header.content_length == 0) {
                std::cerr << "No Content-Length given\n";
                header.content_length = 1024;
            }

            size_t cnt = streambuffer.size();
            streambuffer.prepare(header.content_length);
            try {
                while(cnt < header.content_length) {
                    cnt += boost::asio::read(this->_socket, streambuffer,
                                boost::asio::transfer_at_least(header.content_length - cnt));
                }
            }
            catch(const boost::system::system_error &err) {
                if (err.code() == boost::asio::error::eof) {
                    // EOF is ok.
                    break;
                }
                std::cerr << "Could not read json data: socket error " << err.code() << ":" << err.what() << "\n";
                break;
            }

#ifdef DEBUG_MESSAGETRAFFIC
            std::cout << "RECEIVED: [" << cnt << "]: " << std::string(boost::asio::buffers_begin(streambuffer.data()), boost::asio::buffers_begin(streambuffer.data()) + streambuffer.size());
            std::cout << "\n";
#endif
            std::istream jsonstream(&streambuffer);
            // Now we should have the full json blob in the json stream
            this->handler->handle_message(jsonstream, cnt, this);

            // And for good measure clear up pending messages
            this->clean_pending_messages(std::chrono::seconds(10));
        }
        this->_socket.shutdown(this->_socket.shutdown_both);
        this->_socket.close();
        std::cout << "Done with the socket\n";
        //delete this;
    });
}

void Connection::clean_pending_messages(const std::chrono::system_clock::duration &max_age) {
    auto now = std::chrono::system_clock::now();

    // There is no support for remove_if in associative containers. sometimes I love you C++.
    // If it were, we could say something like
    //pending_messages.erase(std::remove_if(pending_messages.begin(), pending_messages.end(), [&](const std::pair<int, pending_message> &msg) {
    //    return (now - msg.second.pending_since) > max_age;
    //}));
    int cnt = 0;
    for(auto it = pending_messages.begin(); it != pending_messages.end();){
        if ((now - it->second.pending_since) > max_age) {
            it = pending_messages.erase(it); // previously this was something like m_map.erase(it++);
            cnt ++;
        } else {
            ++it;
        }
    }

    if (cnt > 0)
        std::cout << "Cleared " << cnt << "stale messages with a missing response\n";
}

void Connection::default_reporting_message_handler(const ResponseMessage &msg, Connection *, project *) {
    std::cout << "Unhandled message (TODO add more info)\n" << msg.id.value_int << "\n";
}

void Connection::no_reponse_expected(const ResponseMessage &msg, Connection *, project *) {
    if (msg.error) {
        std::cout << "The Request with ID " << msg.id.value_int << " has failed with error code " << static_cast<int>(msg.error->code) << ": \n"
            << msg.error->message << "\n";
    }
}

void Connection::handle_pending_response(const ResponseMessage &msg) {
    if (msg.id.type != RequestId::INT) {
        std::cout << "Received data without a handle-able ID " << msg.id.value_str << "\n";
        return;
    }

    auto it = pending_messages.find(msg.id.value_int);
    if (it != pending_messages.end()) {
        it->second.callback(msg, this, &this->active_project);
        pending_messages.erase(it);
    }
}

void Connection::send(const boost::asio::streambuf &streambuffer) {
    // Send headers
    {
        boost::asio::streambuf headerbuf;
        headerbuf.prepare(32); // Optimistic, size will be ~19 - 25, depending in integer length
        std::ostream headerstream(&headerbuf);
        headerstream << "Content-Length: " << streambuffer.size() << "\r\n";
        headerstream << "\r\n";
        headerstream.flush();
        this->_socket.send(headerbuf.data());
    }

    // Send payload
    size_t cnt_send = this->_socket.send(streambuffer.data());
#ifdef DEBUG_MESSAGETRAFFIC
    std::cout << "SENDING: [" << cnt_send << "]: " 
        << std::string(boost::asio::buffers_begin(streambuffer.data()), boost::asio::buffers_begin(streambuffer.data()) + streambuffer.size());
    std::cout << "\n";
#else
    (void)cnt_send;
#endif
}

void Connection::send(ResponseMessage &msg, const RequestId &id) {
    // Prepare payload
    boost::asio::streambuf streambuffer;
    std::ostream responsestream(&streambuffer);

    if (!msg.id.is_set())
        msg.id = id;

    decode_env env(responsestream);
    env.store(responsestream, msg);

    this->send(streambuffer);
}

void Connection::send(RequestMessage &msg, const std::string &method, const RequestId &id, request_callback_t callback) {
    // Prepare payload
    boost::asio::streambuf streambuffer;
    std::ostream responsestream(&streambuffer);

    if (!msg.id.is_set()) {
        msg.id = id;
    }
    if (msg.id.type == RequestId::AUTO_INCREMENT) {
        msg.id.type = RequestId::INT;
        msg.id.value_int = this->next_request_id;
        this->next_request_id ++;
    }

    if (msg.method.empty()) {
        msg.method = method;
    }

    pending_messages.emplace(std::make_pair(msg.id.value_int, pending_message(callback)));

    decode_env env(responsestream);
    env.store(responsestream, msg);

    this->send(streambuffer);
}


void Connection::send(ResponseResult &result, const RequestId &id) {
    ResponseMessage msg(result);
    this->send(msg, id);
}

void Connection::send(ResponseResult &&result, const RequestId &id) {
    ResponseMessage msg(result);
    this->send(msg, id);
}

void Connection::send(ResponseError &error, const RequestId &id) {
    ResponseMessage msg(nullptr);
    msg.error = error;
    this->send(msg, id);
}

void Connection::send(ResponseError &&error, const RequestId &id) {
    ResponseMessage msg(nullptr);
    msg.error = error;
    this->send(msg, id);
}
