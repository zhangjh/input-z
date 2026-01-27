#include <QApplication>
#include <windows.h>
#include "server_app.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int argc = 0;
    QApplication app(argc, nullptr);
    app.setApplicationName("SuYanServer");
    app.setQuitOnLastWindowClosed(false);

    suyan::ServerApp server;
    if (!server.initialize()) {
        return 1;
    }

    return app.exec();
}
