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

    void run();

private:
    void register_messages();
    void handle_message(std::istream &msg, size_t size, Connection *);

    uint16_t port;

    RequestId active_id;
    std::unordered_map<std::string, std::function<std::unique_ptr<RequestMessage>(decode_env &)>> typemap;

    bool running = true;
};
