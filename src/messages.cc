#include "messages.h"
#include "connection.h"
#include "project.h"

#include <iostream>

#define UNUSED(x) (void)(x)

void InitializeRequest::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    InitializeResult msg;
    std::cout << "Processing InitializeRequest\n";
    // TODO fill in the initialize Result (Capabilities are automatically encoded)

    // TODO initialize the project from the initalization parameters

    conn->send(msg, id);
}

void ShutdownRequest::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    UNUSED(conn);
    UNUSED(id);
    // TODO : Shutdown response
}

void ExitRequest::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    UNUSED(id);
    // TODO do i need a shutdown response?
    conn->close();
}


void DidOpenTextDocument::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    UNUSED(conn);
    // Called when a document is opened
    std::cout << "\nOpened Text document with contents: " << this->textDocument.text << "\n\n";
}

void DidChangeTextDocument::process(Connection *conn, project *proj, const RequestId &id) {
    // Called when a document is opened
    std::cout << "Changed Text document with new contents: " << this->textDocument.text << "\n\n";
}

void DidCloseTextDocument::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    UNUSED(conn);
    // Called when a document is opened
    std::cout << "Closed Text document " << this->textDocument.uri.getPath() << "\n\n";
}

void TextDocumentHover::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    // Called when a document is opened
    std::cout << "Hover over : " << this->textDocument.uri.getPath() << " at " << this->position.line << ":"<< this->position.character << "\n";

    HoverResponse hover;
    hover.contents = "Hello VSCode! I Am Alive, you are at line " + std::to_string(this->position.line);
    hover.range.start = this->position;
    hover.range.end = this->position;

    conn->send(hover, id);

    // TODO change implement properly

    ShowDocumentParams showdoc;
    showdoc.uri = DocumentUri::fromPath("/tmp/test");
    showdoc.external = false;
    showdoc.takeFocus = true;
    showdoc.selection = lsRange();
    showdoc.selection->start.line = 5;
    showdoc.selection->start.character = 5;
    showdoc.selection->end = showdoc.selection->start;
    std::cout << "sending ShowDocument message\n";
    conn->send(showdoc, "window/showDocument", {}, &Connection::no_reponse_expected);

    // Sending test message diagnostic
    // TODO
}

///////////////////////////////////////////////////////////
// OpenSCAD Extensions
///////////////////////////////////////////////////////////

void OpenSCADRender::process(Connection *conn, project *proj, const RequestId &id) {
    UNUSED(proj);
    std::cout << "Starting rendering\n";
}