#include "connection_handler.h"

#include <QThread>
#include <QApplication>

int main(int argc, char **argv) {
    QApplication app (argc, argv);

    ConnectionHandler handler(&app);

    app.exec();
}