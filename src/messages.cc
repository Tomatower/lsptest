#include "messages.h"
#include "connection.h"
#include "project.h"

#include <iostream>

#define UNUSED(x) (void)(x)

void InitializeRequest::process(Connection *conn, project *proj) {
    UNUSED(proj);
    InitializeResult msg;
    std::cout << "Processing InitializeRequest\n";

    conn->send(msg);
}

void ShutdownRequest::process(Connection *conn, project *proj) {
    UNUSED(proj);
    UNUSED(conn);
    // TODO
}

void ExitRequest::process(Connection *conn, project *proj) {
    UNUSED(proj);
    conn->socket().close();
}


void DidOpenTextDocument::process(Connection *conn, project *proj) {
    UNUSED(proj);
    UNUSED(conn);
    // Called when a document is opened
    std::cout << "\nOpened Text document with contents: " << this->textDocument.text << "\n\n";
}

void DidChangeTextDocument::process(Connection *conn, project *proj) {
    // Called when a document is opened
    std::cout << "Changed Text document with new contents: " << this->textDocument.text << "\n\n";
}

void DidCloseTextDocument::process(Connection *conn, project *proj) {
    UNUSED(proj);
    UNUSED(conn);
    // Called when a document is opened
    std::cout << "Closed Text document with new contents: " << this->textDocument.text << "\n\n";
}

void TextDocumentHover::process(Connection *conn, project *proj) {
    UNUSED(proj);
    // Called when a document is opened
    std::cout << "Hover over : " << this->textDocument.getPath() << this->position.line << ":"<< this->position.character << "\n";

    HoverResponse hover;
    hover.contents = "Hello VSCode! I Am Alive, you are at line " + std::to_string(this->position.line);
    hover.range.start = this->position;
    hover.range.end = this->position;

    conn->send(hover);
}

