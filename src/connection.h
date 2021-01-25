#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>

#include "project.h"
#include "lsp.h"

#include <chrono>
#include <unordered_map>

class ConnectionHandler;
class ResponseMessage;
class ResponseResult;
class ResponseError;
class RequestMessage;

class Connection {
public:
    using request_callback_t = std::function<void(const ResponseMessage &, Connection *conn, project *proj)>;

    Connection(boost::asio::io_service &io_service, ConnectionHandler *handler);

    boost::asio::ip::tcp::socket &socket() {
        return this->_socket;
    }

    void handle();

    bool is_done() const {
        return this->running && !this->_socket.is_open();
    }

    static void default_reporting_message_handler(const ResponseMessage &, Connection *, project *);

    // Indicating
    static void no_reponse_expected(const ResponseMessage &, Connection *, project *);


    void send(RequestMessage &message, 
            const std::string &method,
            const RequestId &id,
            request_callback_t = &Connection::default_reporting_message_handler);

    void send(ResponseMessage &message, const RequestId &id);
    void send(ResponseResult &result, const RequestId &id);
    void send(ResponseError &error, const RequestId &id);

    // rvalue-constructed
    void send(ResponseResult &&result, const RequestId &id);
    void send(ResponseError &&error, const RequestId &id);

    // to be called on a regular basis to avoid overfilling the pending messages buffer
    void clean_pending_messages(const std::chrono::system_clock::duration &max_age);
    void handle_pending_response(const ResponseMessage &msg);


    project active_project;
protected:
    void send(const boost::asio::streambuf &buffer);

    bool running = false;
    boost::asio::io_service &io_service;
    boost::asio::ip::tcp::socket _socket;
    ConnectionHandler *handler;

    struct pending_message {
        pending_message(const request_callback_t &callback) :
            callback(callback), pending_since(std::chrono::system_clock::now())
        {}
        request_callback_t callback;
        std::chrono::system_clock::time_point pending_since;
    };
    std::unordered_map<int, pending_message> pending_messages;

    // Used for outgoing requests
    int next_request_id = 0;
};