#pragma once

#include "messages.h"
#include "lsp.h"

#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <cstdint>

// Forward declare Connection in order to speed up compile times (no need to include boost)
class Connection;

class ConnectionHandler {
    /** This is the listener class which creates threads with Connections* running */
    friend class Connection;
public:
    ConnectionHandler(uint16_t port=23725); // 0x5CAD = 23725
    virtual ~ConnectionHandler();

    /**
    * Execute the Connection handler, accepting connections on the given port.
    * 
    * Will block the calling thread. This thread will also be used for the message execution.
    * If any Qt-interaction is to be made, this should be started in a QThread.
    */
    void run();

private:
    // Implemented in decoding.cc - needed for scoping of the decoding template magic.
    void register_messages();
    // Implemented in decoding.cc - needed for scoping of the decoding template magic.
    void handle_message(std::istream &msg, size_t size, Connection *);

    uint16_t port;

    // Since the message handling is single threaded
    RequestId active_id;
    std::unordered_map<std::string, std::function<std::unique_ptr<RequestMessage>(decode_env &)>> typemap;

    bool running = true;
};
