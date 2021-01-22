#include "connection_handler.h"
#include "connection.h"
#include "lsp.h"
#include "messages.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <assert.h>

#include <functional>
#include <iostream>
#include <list>                                                     // for list
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>                                                  // for move

#define UNUSED(x) (void)(x)

/// json Backend: Boost
decode_env::decode_env(std::istream &stream, const size_t size, storage_direction dir) :
        dir(dir)
{
    QByteArray streamdata;
    streamdata.resize(size);
    stream.read(streamdata.data(), size);

    QJsonParseError err;
    this->document = QJsonDocument::fromJson(streamdata);

    if (this->document.isNull()) {
        ResponseError msg(ErrorCode::InvalidRequest,
            std::string("JSON Parse Error at offset: ") + std::to_string(err.offset) + ": " + err.errorString().toStdString());
        throw msg;
    }
}

decode_env::decode_env(std::ostream &stream, storage_direction dir) :
        dir(dir)
{
    UNUSED(stream);
}

void decode_env::store(std::ostream &stream, ResponseMessage &msg) {
    QJsonObject root;
    {
        EncapsulatedObjectRef wrapper(root, storage_direction::WRITE);
        this->declare_field(wrapper, msg, "");
    }
    this->document.setObject(root);
    stream << this->document.toJson(QJsonDocument::JsonFormat::Compact).toStdString();
}


template <>
bool decode_env::declare_field(JSONObject &object, RequestId &target, const FieldNameType &field);
//////////////////////////////////////////////////


void ConnectionHandler::handle_message(std::istream &msg, size_t size, Connection *conn) {
    // Convert to json and construct the message from it
    RequestId id;
    std::string method;
    this->active_id = {};
    try {
        decode_env env(msg, size);
        auto root = env.document.object();
        {
            EncapsulatedObjectRef wrapper(root, storage_direction::READ);
            env.declare_field_default(wrapper, id, "id", {});
            this->active_id = id;

            env.declare_field_default(wrapper, method, "method", {});
            if (method.empty()) {
                std::cout << "ERROR: No Method!\n";
                conn->send(ResponseError(ErrorCode::InvalidRequest, "No Method given"));
                return;
            }
        }

        std::cout << "Handling Message [id " << id.value()  << "] with method " << method << "\n";
        auto it = this->typemap.find(method);
        if (it == this->typemap.end()) {
            std::cerr << "invalid method requested " << method << "\n";
            conn->send(ResponseError(ErrorCode::MethodNotFound, std::string("Method [") + method + "] not implemented"));
            return;
        } else {
            auto decoded_msg = it->second(env);
            decoded_msg->process(conn, &conn->active_project);
        }
    }
    catch (std::unique_ptr<ResponseMessage> &msg) {
        if (!msg->id.is_set()) {
            msg->id = this->active_id;
        }
        std::cout << "Cought response message\n";
        conn->send(*msg);
    }
    catch (std::unique_ptr<ResponseError> &msg) {
        std::cout << "Cought error ptr message: " << msg->message << "\n";
        conn->send(*msg);
    }
    catch (ResponseError &msg) {
        std::cout << "Cought error message: " << msg.message << "\n";
        conn->send(msg);
    }
    catch(std::exception &err) {
        std::cerr << "cought std::exception during message handling: " << err.what() << "\n";
        conn->send(ResponseError(ErrorCode::InternalError, err.what()));
    }
    /*catch(...) {
        conn->send(ResponseError(ErrorCode::InternalError, "Unspecified internal error"));
    }*/
}

void ConnectionHandler::register_messages() {
    std::cout << "Method mapping:\n";

    #define MAP(method, messagetype) do {\
        this->typemap.emplace(method, [](decode_env &env)->std::unique_ptr<RequestMessage> { \
            static_assert(std::is_base_of<RequestMessage, messagetype>::value); \
            auto resp = std::make_unique<messagetype>(); \
            auto root = env.document.object(); \
            { \
            EncapsulatedChildObjectRef wrapper(root, "params", storage_direction::READ); \
            env.declare_field(wrapper, *resp, ""); \
            } \
            return resp; \
        }); \
        std::cout << "\t" << method << " \t --> " << #messagetype "\n"; \
    } while (0)

    // Define Messages here
    MAP("initialize", InitializeRequest);
    MAP("initialized", InitializedNotifiy);
    MAP("shutdown", ShutdownRequest);
    MAP("textDocument/didOpen", DidOpenTextDocument);
    MAP("textDocument/didChange", DidChangeTextDocument);
    MAP("textDocument/hover", TextDocumentHover);



    #undef MAP
}



/////////////////////////////////////////////////////////////////////
// LSP Basic Structures
/////////////////////////////////////////////////////////////////////
template <>
bool decode_env::declare_field(JSONObject &object, RequestId &target, const FieldNameType &field) {
    auto data = object.ref()[field];
    if (dir == storage_direction::READ) {
        if (data.type() == QJsonValue::String) {
            declare_field(object, target.value_str, field);
            target.type = RequestId::STRING;
        } else if (data.type() == QJsonValue::Double) {
            declare_field(object, target.value_int, field);
            target.type = RequestId::INT;
        }
    } else {
        switch(target.type) {
        case RequestId::STRING:
            declare_field(object, target.value_str, field);
            break;
        case RequestId::INT:
            declare_field(object, target.value_int, field);
            break;
        case RequestId::UNSET:
            break;
        }
    }
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &object, ErrorCode &errorcode, const FieldNameType &field) {
    int interror = static_cast<int>(errorcode);
    declare_field(object, interror, field);
    if (this->dir == storage_direction::WRITE) {
        errorcode = static_cast<ErrorCode>(interror);
    }
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &object, DocumentUri &target, const FieldNameType &field) {
    return declare_field(object, target.raw_uri, field);
}

template <>
bool decode_env::declare_field(JSONObject &parent, Position &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.line, "line");
    declare_field(object, target.character, "character");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, lsRange &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.start, "start");
    declare_field(object, target.end, "end");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, Location &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.uri, "uri");
    declare_field(object, target.range, "range");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, LocationLink &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.targetUri, "targetUri");
    declare_field(object, target.targetRange, "targetRange");
    declare_field(object, target.targetSelectionRange, "targetSelectionRange");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &object, SymbolKind &target, const FieldNameType &field) {
    uint8_t symbol = static_cast<uint8_t>(target);
    declare_field(object, symbol, field);
    if (this->dir == storage_direction::WRITE) {
        target = static_cast<SymbolKind>(symbol);
    }
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, SymbolInformation &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.name, "name");
    declare_field(object, target.kind, "kind");
    declare_field(object, target.location, "location");
    declare_field_optional(object, target.containerName, "containerName");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, TextDocumentIdentifier &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.uri, "uri");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, VersionedTextDocumentIdentifier &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.uri, "uri");
    declare_field_optional(object, target.version, "version");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, TextEdit &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.range, "range");
    declare_field(object, target.newText, "newText");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, TextDocumentItem &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.uri, "uri");
    declare_field(object, target.languageId, "languageId");
    declare_field(object, target.version, "version");
    declare_field(object, target.text, "text");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, TextDocumentContentChangeEvent &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    //declare_field_optional(object, target.range, "range");
    //declare_field_optional(object, target.rangeLength, "rangeLength");
    declare_field(object, target.text, "text");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, WorkDoneProgress &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.kind, "kind");
    declare_field_optional(object, target.title, "title");
    declare_field_optional(object, target.message, "message");
    declare_field_optional(object, target.percentage, "percentage");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, WorkDoneProgressParam &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.token, "token");
    declare_field(object, target.value, "value");
    return true;
}

template <>
bool decode_env::declare_field(JSONObject &parent, WorkspaceFolder &target, const FieldNameType &field) {
    auto object = start_object(parent, field);
    declare_field(object, target.uri, "uri");
    declare_field(object, target.name, "name");
    return true;
}

//////////////////////////////////////////////////////////////////////
// LSP Base Protocol
//////////////////////////////////////////////////////////////////////

template<>
bool decode_env::declare_field(JSONObject &parent, ResponseError &target, const FieldNameType &) {
    declare_field(parent, target.code, "code");
    declare_field(parent, target.message, "message");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, ResponseResult &target, const FieldNameType &field) {
    target.decode(*this, object, field); // For Polymorphism
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &parent, RequestMessage &target, const FieldNameType &) {
    auto object = start_object(parent, "params");
    target.decode(*this, object, "");  // For Polymorphism
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, ResponseMessage &target, const FieldNameType &) {
    assert(this->dir != storage_direction::READ);
    std::string jsonprocversion = "2.0";
    declare_field(object, jsonprocversion, "jsonrpc");
    declare_field(object, target.id, "id");

    if (this->dir == storage_direction::WRITE) {
        if (target.error) {
            auto errorenv = start_object(object, "error");
            declare_field(errorenv, *target.error, "error");
        } else if (target.result) {
            auto resultenv = start_object(object, "result");
            declare_field(resultenv, *target.result, "result");
        } else {
            std::cerr << "Having a message with neither error nor result\n";
        }
    } else {
        std::cerr << "ResponseMessage will never be read";
        assert(false);
    }
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, InitializeRequest &target, const FieldNameType &) {
    // TODO

    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, InitializeResult &target, const FieldNameType &) {
    declare_field(object, target.capabilities, "capabilities");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, ServerCapabilities &target, const FieldNameType &field) {
    assert(this->dir != storage_direction::READ); // The following assignment code does not allow reading!


    object[field] = QJsonObject {
        {"hoverProvider", true},
        {"textDocumentSync", QJsonObject {
                {"openClose", true },
                {"change", 1 }, // None = 0, Full = 1, Incremental = 2
            },
        },
    };
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, InitializedNotifiy &target, const FieldNameType &) {
    // TODO
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &, ShutdownRequest &, const FieldNameType &) {
    // Does not have fields
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &, ExitRequest &, const FieldNameType &) {
    // Does not have fields
    return true;
}

///////////////////////////////////////////////////////////
// LSP General Messages
///////////////////////////////////////////////////////////
template<>
bool decode_env::declare_field(JSONObject &object, DidOpenTextDocument &target, const FieldNameType &) {
    declare_field(object, target.textDocument, "textDocument");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, DidChangeTextDocument &target, const FieldNameType &) {
    declare_field(object, target.textDocument, "textDocument");
    declare_field(object, target.contentChanges, "contentChanges");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, DidCloseTextDocument &target, const FieldNameType &) {
    declare_field(object, target.textDocument, "textDocument");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, TextDocumentPositionParams &target, const FieldNameType &) {
    declare_field(object, target.textDocument, "textDocument");
    declare_field(object, target.position, "position");
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, TextDocumentHover &target, const FieldNameType &field) {
    declare_field(object, (TextDocumentPositionParams &)target, field);
    return true;
}

template<>
bool decode_env::declare_field(JSONObject &object, HoverResponse &target, const FieldNameType &) {
    declare_field(object, target.contents, "contents");
    declare_field(object, target.range, "range");
    return true;
}
