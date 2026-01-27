#pragma once

#include <windows.h>
#include <string>

namespace suyan {

#define SUYAN_IPC_PIPE_NAME L"SuYanInputMethod"

enum IPCCommand {
    IPC_ECHO = 1,
    IPC_START_SESSION,
    IPC_END_SESSION,
    IPC_PROCESS_KEY,
    IPC_TEST_KEY,
    IPC_FOCUS_IN,
    IPC_FOCUS_OUT,
    IPC_UPDATE_POSITION,
    IPC_COMMIT,
    IPC_CLEAR,
    IPC_SELECT_CANDIDATE,
    IPC_SHUTDOWN
};

struct IPCMessage {
    IPCCommand cmd;
    DWORD sessionId;
    DWORD param1;
    DWORD param2;
};

struct IPCResponse {
    DWORD result;
    DWORD dataSize;
};

class IPCClient {
public:
    IPCClient();
    ~IPCClient();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return m_pipe != INVALID_HANDLE_VALUE; }
    
    DWORD startSession();
    void endSession();
    bool testKey(DWORD keyCode, DWORD modifiers);
    bool processKey(DWORD keyCode, DWORD modifiers);
    void updatePosition(const RECT& rc);
    void focusIn();
    void focusOut();
    bool getCommitText(std::wstring& text);
    bool getPreeditText(std::wstring& text);
    
private:
    DWORD send(IPCCommand cmd, DWORD p1 = 0, DWORD p2 = 0);
    bool readData(std::wstring& data);
    
    HANDLE m_pipe;
    DWORD m_sessionId;
    char m_buffer[8192];
};

} // namespace suyan
