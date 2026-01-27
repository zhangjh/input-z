#pragma once

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPoint>
#include <memory>
#include "ipc_channel.h"

namespace suyan {

class InputEngine;
class CandidateWindow;
struct InputState;

class ServerApp : public QObject {
    Q_OBJECT
public:
    explicit ServerApp(QObject* parent = nullptr);
    ~ServerApp();

    bool initialize();
    void shutdown();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onQuit();

private:
    DWORD handleIPCRequest(const IPCMessage& msg, std::wstring& response);
    bool shouldProcessKey(int keyCode, int modifiers);
    int convertVirtualKeyToRimeKey(int vk, int modifiers);
    void setupTrayIcon();

    std::unique_ptr<IPCServer> m_ipcServer;
    std::unique_ptr<InputEngine> m_inputEngine;
    std::unique_ptr<CandidateWindow> m_candidateWindow;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayMenu;
    std::wstring m_commitText;
    QPoint m_cursorPos;
};

} // namespace suyan
