#include "ipc_client.h"
#include <algorithm>
#include <fstream>

namespace suyan {

static void DebugLog(const char* msg) {
    std::ofstream f("C:\\temp\\suyan32_debug.log", std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
    }
}

static std::wstring GetPipeName() {
    return L"\\\\.\\pipe\\SuYanInputMethod";
}

IPCClient::IPCClient() 
    : m_pipe(INVALID_HANDLE_VALUE)
    , m_sessionId(0) {
    memset(m_buffer, 0, sizeof(m_buffer));
}

IPCClient::~IPCClient() {
    disconnect();
}

bool IPCClient::connect() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        DebugLog("connect: already connected");
        return true;
    }
    
    std::wstring pipeName = GetPipeName();
    DebugLog("connect: trying to connect...");
    
    m_pipe = CreateFileW(
        pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (m_pipe != INVALID_HANDLE_VALUE) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(m_pipe, &mode, nullptr, nullptr);
        DebugLog("connect: SUCCESS");
        return true;
    }
    
    char buf[64];
    sprintf_s(buf, "connect: FAILED, error=%lu", GetLastError());
    DebugLog(buf);
    return false;
}

void IPCClient::disconnect() {
    if (m_sessionId != 0) {
        endSession();
    }
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}

DWORD IPCClient::send(IPCCommand cmd, DWORD p1, DWORD p2) {
    if (!isConnected()) {
        DebugLog("send: not connected");
        return 0;
    }
    
    char buf[128];
    sprintf_s(buf, "send: cmd=%d, p1=%lu, p2=%lu, session=%lu", (int)cmd, p1, p2, m_sessionId);
    DebugLog(buf);
    
    IPCMessage msg = { cmd, m_sessionId, p1, p2 };
    DWORD written = 0;
    
    if (!WriteFile(m_pipe, &msg, sizeof(msg), &written, nullptr)) {
        sprintf_s(buf, "send: WriteFile FAILED, error=%lu", GetLastError());
        DebugLog(buf);
        disconnect();
        return 0;
    }
    
    FlushFileBuffers(m_pipe);
    
    IPCResponse resp = {};
    DWORD read = 0;
    
    if (!ReadFile(m_pipe, &resp, sizeof(resp), &read, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            sprintf_s(buf, "send: ReadFile FAILED, error=%lu", GetLastError());
            DebugLog(buf);
            disconnect();
            return 0;
        }
    }
    
    sprintf_s(buf, "send: result=%lu, dataSize=%lu", resp.result, resp.dataSize);
    DebugLog(buf);
    
    return resp.result;
}

bool IPCClient::readData(std::wstring& data) {
    DWORD read = 0;
    memset(m_buffer, 0, sizeof(m_buffer));
    
    if (!ReadFile(m_pipe, m_buffer, sizeof(m_buffer) - 2, &read, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            return false;
        }
    }
    
    if (read > 0) {
        data = reinterpret_cast<wchar_t*>(m_buffer);
    }
    return true;
}

DWORD IPCClient::startSession() {
    DebugLog("startSession called");
    m_sessionId = send(IPC_START_SESSION);
    char buf[64];
    sprintf_s(buf, "startSession: got sessionId=%lu", m_sessionId);
    DebugLog(buf);
    return m_sessionId;
}

void IPCClient::endSession() {
    send(IPC_END_SESSION);
    m_sessionId = 0;
}

bool IPCClient::testKey(DWORD keyCode, DWORD modifiers) {
    char buf[64];
    sprintf_s(buf, "testKey: key=%lu, mod=%lu", keyCode, modifiers);
    DebugLog(buf);
    bool result = send(IPC_TEST_KEY, keyCode, modifiers) != 0;
    sprintf_s(buf, "testKey: result=%d", result ? 1 : 0);
    DebugLog(buf);
    return result;
}

bool IPCClient::processKey(DWORD keyCode, DWORD modifiers) {
    char buf[64];
    sprintf_s(buf, "processKey: key=%lu, mod=%lu", keyCode, modifiers);
    DebugLog(buf);
    bool result = send(IPC_PROCESS_KEY, keyCode, modifiers) != 0;
    sprintf_s(buf, "processKey: result=%d", result ? 1 : 0);
    DebugLog(buf);
    return result;
}

void IPCClient::updatePosition(const RECT& rc) {
    DWORD x = static_cast<DWORD>(rc.left);
    DWORD y = static_cast<DWORD>(rc.bottom);
    send(IPC_UPDATE_POSITION, x, y);
}

void IPCClient::focusIn() {
    send(IPC_FOCUS_IN);
}

void IPCClient::focusOut() {
    send(IPC_FOCUS_OUT);
}

bool IPCClient::getCommitText(std::wstring& text) {
    if (send(IPC_COMMIT) == 0) {
        return false;
    }
    return readData(text);
}

bool IPCClient::getPreeditText(std::wstring& text) {
    return readData(text);
}

} // namespace suyan
