// Copyright 2017-2018 ccls Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <optional>
#include <vector>
#include <string>

struct RequestId {
    enum { UNSET, STRING, INT } type = UNSET;
    std::string value_str;
    int value_int;

    bool is_set() { return type != UNSET; }
    std::string value() {
        switch(type) {
        case STRING:
            return value_str;
        case INT:
            return std::to_string(value_int);
        case UNSET:
            return "<UNSET>";
        }

    }
};

enum class ErrorCode {
  // Defined by JSON RPC
  ParseError = -32700,
  InvalidRequest = -32600,
  MethodNotFound = -32601,
  InvalidParams = -32602,
  InternalError = -32603,
  serverErrorStart = -32099,
  serverErrorEnd = -32000,
  ServerNotInitialized = -32002,
  UnknownErrorCode = -32001,

  // Defined by the protocol.
  RequestCancelled = -32800,
};

struct DocumentUri {
  static DocumentUri fromPath(const std::string &path);

  bool operator==(const DocumentUri &o) const { return raw_uri == o.raw_uri; }
  bool operator<(const DocumentUri &o) const { return raw_uri < o.raw_uri; }

  void setPath(const std::string &path) { raw_uri = path; };
  std::string getPath() const { return raw_uri; }

  std::string raw_uri;
};

struct Position {
  int line = 0;
  int character = 0;
  bool operator==(const Position &o) const {
    return line == o.line && character == o.character;
  }
  bool operator<(const Position &o) const {
    return line != o.line ? line < o.line : character < o.character;
  }
  bool operator<=(const Position &o) const {
    return line != o.line ? line < o.line : character <= o.character;
  }
  std::string toString() const;
};

struct lsRange {
  Position start;
  Position end;
  bool operator==(const lsRange &o) const {
    return start == o.start && end == o.end;
  }
  bool operator<(const lsRange &o) const {
    return !(start == o.start) ? start < o.start : end < o.end;
  }
  bool includes(const lsRange &o) const {
    return start <= o.start && o.end <= end;
  }
  bool intersects(const lsRange &o) const {
    return start < o.end && o.start < end;
  }
};

struct Location {
  DocumentUri uri;
  lsRange range;
  bool operator==(const Location &o) const {
    return uri == o.uri && range == o.range;
  }
  bool operator<(const Location &o) const {
    return !(uri == o.uri) ? uri < o.uri : range < o.range;
  }
};

struct LocationLink {
  std::string targetUri;
  lsRange targetRange;
  lsRange targetSelectionRange;
  explicit operator bool() const { return targetUri.size(); }
  explicit operator Location() && {
    return {DocumentUri{std::move(targetUri)}, targetSelectionRange};
  }
  bool operator==(const LocationLink &o) const {
    return targetUri == o.targetUri &&
           targetSelectionRange == o.targetSelectionRange;
  }
  bool operator<(const LocationLink &o) const {
    return !(targetUri == o.targetUri)
               ? targetUri < o.targetUri
               : targetSelectionRange < o.targetSelectionRange;
  }
};

enum class SymbolKind : uint8_t {
  Unknown = 0,

  File = 1,
  Module = 2,
  Namespace = 3,
  Package = 4,
  Class = 5,
  Method = 6,
  Property = 7,
  Field = 8,
  Constructor = 9,
  Enum = 10,
  Interface = 11,
  Function = 12,
  Variable = 13,
  Constant = 14,
  String = 15,
  Number = 16,
  Boolean = 17,
  Array = 18,
  Object = 19,
  Key = 20,
  Null = 21,
  EnumMember = 22,
  Struct = 23,
  Event = 24,
  Operator = 25,
};

struct SymbolInformation {
  std::string name;
  SymbolKind kind;
  Location location;
  std::optional<std::string> containerName;
};

struct TextDocumentIdentifier {
  DocumentUri uri;
};

struct VersionedTextDocumentIdentifier {
  DocumentUri uri;
  // The version number of this document.  number | null
  std::optional<int> version;
};

struct TextEdit {
  lsRange range;
  std::string newText;
};

struct TextDocumentItem {
  DocumentUri uri;
  std::string languageId;
  int version;
  std::string text;
};

struct TextDocumentContentChangeEvent {
  // The range of the document that changed.
  std::optional<lsRange> range;
  // The length of the range that got replaced.
  std::optional<int> rangeLength;
  // The new text of the range/document.
  std::string text;
};

struct TextDocumentDidChangeParam {
  VersionedTextDocumentIdentifier textDocument;
  std::vector<TextDocumentContentChangeEvent> contentChanges;
};

struct WorkDoneProgress {
  std::string kind;
  std::optional<std::string> title;
  std::optional<std::string> message;
  std::optional<int> percentage;
};
struct WorkDoneProgressParam {
  std::string token;
  WorkDoneProgress value;
};

struct WorkspaceFolder {
  DocumentUri uri;
  std::string name;
};

