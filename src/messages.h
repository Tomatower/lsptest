#pragma once

#include "lsp.h"
#include "project.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <istream>
#include <memory>
#include <string>
#include <iostream>


class Connection;
struct decode_env;
class ResponseMessage;

enum class storage_direction {
    READ, WRITE
};

using FieldNameType = QString;

/**
 * This class helps to avoid the inconvienience that we cannot construct Qt JSON Object trees from the bottom up.
 * They have to be constructed from leaf to root, so we create an interface how we insert a child into the tree
 * after it has been fully constructed
 */
class EncapsulatedObjectRef {
protected:
    QJsonObject &object;
    const storage_direction direction;

public:
    EncapsulatedObjectRef(QJsonObject &object, const storage_direction direction) :
        object(object), direction(direction) {}

    EncapsulatedObjectRef(EncapsulatedObjectRef &&rhs) :
        object(rhs.object), direction(rhs.direction) {}

    virtual QJsonObject *operator->() { return &object; }
    virtual QJsonObject *operator*() { return &object; }
    virtual QJsonValueRef operator[](const FieldNameType &t) { return this->object[t]; }

    virtual operator QJsonObject &() { return this->object; }

    virtual QJsonObject &ref() { return this->object; }

    virtual ~EncapsulatedObjectRef() {}
};


/**
 * Just like EncapsulatedObjectRef but add the child upon destruction.
 * Use the Object from EncapsulatedObjectRef as parent object
 */
class EncapsulatedChildObjectRef : public EncapsulatedObjectRef {
    const FieldNameType field;
    QJsonObject child;
public:
    EncapsulatedChildObjectRef(QJsonObject &parent, const FieldNameType &field, const storage_direction dir) :
        EncapsulatedObjectRef(parent, dir),
        field(field),
        child((dir==storage_direction::WRITE)? parent[field].toObject() : QJsonObject{})
    {}

    EncapsulatedChildObjectRef(EncapsulatedChildObjectRef &&rhs) :
        EncapsulatedObjectRef(std::move(rhs)),
        field(std::move(rhs.field)),
        child(std::move(rhs.child))
    {}

    QJsonObject *operator->() { return &child; }
    QJsonObject *operator*() { return &child; }
    QJsonValueRef operator[](const FieldNameType &t) { return this->child[t]; }

    operator QJsonObject &() { return this->child; }

    QJsonObject &ref() { return this->child; }

    virtual ~EncapsulatedChildObjectRef() {
        if (this->direction == storage_direction::WRITE && !this->child.empty()) { //isNull
            this->object[this->field] = this->child;
        }
    }
};


using JSONObject = EncapsulatedObjectRef;

struct decode_env {
    const storage_direction dir;
    QJsonDocument document;

    decode_env(std::istream &stream, size_t size, storage_direction dir=storage_direction::READ);
    decode_env(std::ostream &stream, storage_direction dir=storage_direction::WRITE);

    void store(std::ostream &stream, ResponseMessage &);

    template<typename value_type>
    bool declare_field(JSONObject &parent, value_type &dst, const FieldNameType &field) {
        //assert(parent.isObject());
        if (this->dir == storage_direction::READ)  {
            auto object = parent.ref();
            auto it = object.find(field);
            if (it != object.end()) {
                if constexpr (std::is_integral<value_type>::value) {
                    dst = it->toInt();
                } else if constexpr(std::is_floating_point<value_type>::value) {
                    dst = it->toDouble();
                } else if constexpr(std::is_same<value_type, bool>::value) {
                    dst = it->toBool();
                } else if constexpr(std::is_convertible<value_type, std::string>::value) {
                    dst = it->toString().toStdString();
                } else if constexpr(std::is_convertible<value_type, QString>::value) {
                    dst = it->toString();
                } else {
                    std::cerr << "Trying to decode unknown type\n";
                    printf("%i", dst);
                    //static_assert(std::is_same<value_type, bool>::value);
                }
                return true;
            } else {
                return false;
            }
        } else {
            if constexpr(std::is_convertible<value_type, std::string>::value) {
                parent[field] = QString::fromStdString(dst);
            } else {
                parent[field] = dst;
            }
            return true;
        }
    }

    template <typename optional_type>
    bool declare_field_optional(JSONObject &object, std::optional<optional_type> &target, const FieldNameType &field) {
        if (dir == storage_direction::READ) {
            optional_type t;
            declare_field(object, t, field);
            target = t;
        } else {
            if (target) {
                optional_type t = target.value();
                declare_field(object, t, field);
            }
        }
    }

    template<typename value_type>
    bool declare_field_default(JSONObject &object, value_type &dst, const FieldNameType &field, const value_type &dflt) {
        if (!this->declare_field(object, dst, field)) {
            dst = dflt;
            return false;
        }
        return true;
    }

    EncapsulatedChildObjectRef start_object(JSONObject &parent, const FieldNameType &field) {
        return EncapsulatedChildObjectRef(parent, field, this->dir);
    }
};

#define MAKE_DECODEABLE \
virtual void decode(decode_env &env, JSONObject &object, const FieldNameType &field) { env.declare_field(object, *this, field); } \

#define MESSAGE_CLASS(MESSAGETYPE) \
struct MESSAGETYPE; \
template<> \
bool decode_env::declare_field(JSONObject &object, MESSAGETYPE &target, const FieldNameType &field); \
struct MESSAGETYPE






struct RequestMessage {

    RequestId id;
    std::string method;

    virtual void process(Connection *conn, project *project) = 0;
    virtual void decode(decode_env &env, JSONObject &object, const FieldNameType &field) = 0;

    virtual ~RequestMessage() {}
};

// Base Class for Results
MESSAGE_CLASS(ResponseResult) {
    MAKE_DECODEABLE;

    virtual ~ResponseResult() {}
};

MESSAGE_CLASS(ResponseError) {
    MAKE_DECODEABLE;
    ResponseError(ErrorCode err, const std::string &msg) :
        code(err), message(msg) {}

    ErrorCode code;
    std::string message;

    virtual ~ResponseError() {}
};


MESSAGE_CLASS(ResponseMessage) {
    MAKE_DECODEABLE;
    RequestId id;
    ResponseResult *result = nullptr;
    ResponseError *error = nullptr;

    virtual ~ResponseMessage() {}
};

///////////////////////////////////////////////////////////
// Begin Interaction Messages
///////////////////////////////////////////////////////////
MESSAGE_CLASS(InitializeRequest) : public RequestMessage {
    MAKE_DECODEABLE;
    virtual void process(Connection *, project *);

    std::string rootUri;
    std::string rootPath;

    // Not used, here for completion
    // Config config;
    // ClientCap capabilities;

    std::vector<WorkspaceFolder> workspaceFolders;
};

MESSAGE_CLASS(ServerCapabilities) {
    MAKE_DECODEABLE;
};

MESSAGE_CLASS(InitializeResult) : public ResponseResult {
    MAKE_DECODEABLE;

    ServerCapabilities capabilities;
};

MESSAGE_CLASS(InitializedNotifiy) : public RequestMessage {
    MAKE_DECODEABLE;
    virtual void process(Connection *, project *){};
};

MESSAGE_CLASS(ShutdownRequest) : public RequestMessage {
    MAKE_DECODEABLE;

    virtual void process(Connection *, project *);
};

MESSAGE_CLASS(ExitRequest) : public RequestMessage {
    MAKE_DECODEABLE;

    virtual void process(Connection *, project *);
};


///////////////////////////////////////////////////////////
// LSP Messages based on capabilities
/// capability: hoverProvider
MESSAGE_CLASS(TextDocumentPositionParams) : public RequestMessage {
    MAKE_DECODEABLE;

    Position position;
    DocumentUri textDocument;
};

/// capability: textDocumentSync
MESSAGE_CLASS(DidOpenTextDocument) : public RequestMessage {
    MAKE_DECODEABLE;

    TextDocumentItem textDocument;
    virtual void process(Connection *, project *);
};

MESSAGE_CLASS(DidChangeTextDocument) : public RequestMessage {
    MAKE_DECODEABLE;

    TextDocumentItem textDocument;

    // Todo handle lists?
    TextDocumentContentChangeEvent contentChanges;

    virtual void process(Connection *, project *);
};

MESSAGE_CLASS(DidCloseTextDocument) : public RequestMessage {
    MAKE_DECODEABLE;

    TextDocumentItem textDocument;
    virtual void process(Connection *, project *);
};

/// capability: hoverProvider
MESSAGE_CLASS(TextDocumentHover) : public TextDocumentPositionParams {
    MAKE_DECODEABLE;

    virtual void process(Connection *, project *);
};

MESSAGE_CLASS(HoverResponse) : public ResponseResult {
    MAKE_DECODEABLE;

    std::string contents;
    lsRange range;
};

#undef MESSAGE_CLASS
#undef MAKE_DECODEABLE
