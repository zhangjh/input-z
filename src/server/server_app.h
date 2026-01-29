#pragma once

#include <QObject>
#include <QPoint>
#include <memory>
#include "ipc_channel.h"
#include "ipc_protocol.h"

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

private:
    DWORD handleIPCRequest(const IPCMessage& msg, std::wstring& response);
    bool shouldProcessKey(int keyCode, int modifiers);
    int convertVirtualKeyToRimeKey(int vk, int modifiers);

    std::unique_ptr<IPCServer> m_ipcServer;
    std::unique_ptr<InputEngine> m_inputEngine;
    std::unique_ptr<CandidateWindow> m_candidateWindow;
    std::wstring m_commitText;
    QPoint m_cursorPos;
    int m_cursorHeight = 20;
};

} // namespace suyan
