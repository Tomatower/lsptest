#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "project.h"

class ConnectionHandler;
class ResponseMessage;
class ResponseResult;
class ResponseError;

class Connection {
public:
    Connection(boost::asio::io_service &io_service, ConnectionHandler *handler);

    boost::asio::ip::tcp::socket &socket() {
        return this->_socket;
    }

    void handle();

    bool is_done() const {
        return this->running && !this->_socket.is_open();
    }

    void send(ResponseMessage &message);
    void send(ResponseResult &result);
    void send(ResponseError &error);

    // rvalue-constructed
    void send(ResponseResult &&result);
    void send(ResponseError &&error);


    project active_project;
protected:
    bool running = false;
    boost::asio::io_service &io_service;
    boost::asio::ip::tcp::socket _socket;
    ConnectionHandler *handler;
};