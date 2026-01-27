#pragma once

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <functional>
#include <memory>

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

std::wstring GetPipeName();

class IPCClient {
public:
    IPCClient();
    ~IPCClient();
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    DWORD sendMessage(IPCCommand cmd, DWORD sessionId, DWORD p1 = 0, DWORD p2 = 0);
    bool readResponse(std::wstring& data);
    
private:
    HANDLE m_pipe;
    std::unique_ptr<char[]> m_buffer;
    static constexpr size_t BUFFER_SIZE = 8192;
};

class IPCServer {
public:
    using RequestHandler = std::function<DWORD(const IPCMessage&, std::wstring&)>;
    
    IPCServer();
    ~IPCServer();
    
    bool start();
    void stop();
    void setHandler(RequestHandler handler);
    
private:
    void serverThread();
    void handleClient(HANDLE pipe);
    
    HANDLE m_pipe;
    RequestHandler m_handler;
    bool m_running;
    HANDLE m_thread;
    SECURITY_ATTRIBUTES m_sa;
};

} // namespace suyan

#endif // _WIN32
