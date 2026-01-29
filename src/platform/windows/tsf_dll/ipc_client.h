#pragma once

#include <windows.h>
#include <string>
#include "../../../shared/ipc_protocol.h"

namespace suyan {

class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    bool connect();
    void disconnect();
    bool isConnected() const { return m_pipe != INVALID_HANDLE_VALUE; }
    bool ensureServer();

    uint32_t startSession();
    void endSession();
    bool testKey(uint32_t vk, uint32_t modifiers);
    bool processKey(uint32_t vk, uint32_t modifiers);
    bool getCommitText(std::wstring& text);
    void updatePosition(int x, int y, int height);
    void focusIn();
    void focusOut();

private:
    uint32_t send(IPCCommand cmd, uint32_t p1 = 0, uint32_t p2 = 0);
    bool readData(std::wstring& data);

    HANDLE m_pipe;
    uint32_t m_sessionId;
    wchar_t m_buffer[4096];
};

} // namespace suyan
