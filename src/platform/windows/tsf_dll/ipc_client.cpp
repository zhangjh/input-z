#include "ipc_client.h"
#include <shellapi.h>

namespace suyan {

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
        return true;
    }

    m_pipe = CreateFileW(
        SUYAN_PIPE_NAME,
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
        return true;
    }

    return false;
}

void IPCClient::disconnect() {
    if (m_sessionId != 0) {
        send(IPC_END_SESSION);
        m_sessionId = 0;
    }
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::ensureServer() {
    if (connect()) {
        return true;
    }

    wchar_t serverPath[MAX_PATH] = {};
    DWORD pathSize = sizeof(serverPath);
    
    LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SuYan",
        L"InstallLocation",
        RRF_RT_REG_SZ,
        nullptr,
        serverPath,
        &pathSize
    );

    if (status != ERROR_SUCCESS || serverPath[0] == L'\0') {
        return false;
    }

    size_t len = wcslen(serverPath);
    if (len > 0 && serverPath[len - 1] != L'\\') {
        wcscat_s(serverPath, L"\\");
    }
    wcscat_s(serverPath, L"SuYanServer.exe");

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = serverPath;
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        return false;
    }

    if (sei.hProcess) {
        CloseHandle(sei.hProcess);
    }

    for (int i = 0; i < 20; ++i) {
        Sleep(100);
        if (connect()) {
            return true;
        }
    }

    return false;
}

uint32_t IPCClient::send(IPCCommand cmd, uint32_t p1, uint32_t p2) {
    if (!isConnected()) {
        return 0;
    }

    IPCMessage msg = { cmd, m_sessionId, p1, p2 };
    DWORD written = 0;

    if (!WriteFile(m_pipe, &msg, sizeof(msg), &written, nullptr)) {
        disconnect();
        return 0;
    }

    IPCResponse resp = {};
    DWORD bytesRead = 0;

    if (!ReadFile(m_pipe, &resp, sizeof(resp), &bytesRead, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            disconnect();
            return 0;
        }
    }

    return resp.result;
}

bool IPCClient::readData(std::wstring& data) {
    DWORD bytesRead = 0;
    memset(m_buffer, 0, sizeof(m_buffer));

    if (!ReadFile(m_pipe, m_buffer, sizeof(m_buffer) - sizeof(wchar_t), &bytesRead, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            return false;
        }
    }

    if (bytesRead > 0) {
        data = m_buffer;
    }
    return true;
}

uint32_t IPCClient::startSession() {
    m_sessionId = send(IPC_START_SESSION);
    return m_sessionId;
}

void IPCClient::endSession() {
    send(IPC_END_SESSION);
    m_sessionId = 0;
}

bool IPCClient::testKey(uint32_t vk, uint32_t modifiers) {
    if (!isConnected() && !ensureServer()) {
        return false;
    }
    return send(IPC_TEST_KEY, vk, modifiers) != 0;
}

bool IPCClient::processKey(uint32_t vk, uint32_t modifiers) {
    if (!isConnected() && !ensureServer()) {
        return false;
    }
    return send(IPC_PROCESS_KEY, vk, modifiers) != 0;
}

bool IPCClient::getCommitText(std::wstring& text) {
    if (send(IPC_COMMIT) == 0) {
        return false;
    }
    return readData(text);
}

void IPCClient::updatePosition(int x, int y, int height) {
    int16_t sx = static_cast<int16_t>(x);
    int16_t sy = static_cast<int16_t>(y);
    uint32_t packed = packCoordinates(sx, sy);
    send(IPC_UPDATE_POSITION, packed, static_cast<uint32_t>(height));
}

void IPCClient::focusIn() {
    send(IPC_FOCUS_IN);
}

void IPCClient::focusOut() {
    send(IPC_FOCUS_OUT);
}

} // namespace suyan
