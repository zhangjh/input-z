#ifdef _WIN32

#include "ipc_channel.h"
#include <sddl.h>

namespace suyan {

std::wstring GetPipeName() {
    return L"\\\\.\\pipe\\SuYanInputMethod";
}

// ============================================
// IPCClient
// ============================================

IPCClient::IPCClient() 
    : m_pipe(INVALID_HANDLE_VALUE)
    , m_buffer(std::make_unique<char[]>(BUFFER_SIZE)) {
}

IPCClient::~IPCClient() {
    disconnect();
}

bool IPCClient::connect() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        return true;
    }
    
    std::wstring pipeName = GetPipeName();
    
    for (int retry = 0; retry < 3; ++retry) {
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
            return true;
        }
        
        if (GetLastError() == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(pipeName.c_str(), 1000);
        } else {
            break;
        }
    }
    
    return false;
}

void IPCClient::disconnect() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::isConnected() const {
    return m_pipe != INVALID_HANDLE_VALUE;
}

DWORD IPCClient::sendMessage(IPCCommand cmd, DWORD sessionId, DWORD p1, DWORD p2) {
    if (!isConnected() && !connect()) {
        return 0;
    }
    
    IPCMessage msg = { cmd, sessionId, p1, p2 };
    DWORD written = 0;
    
    if (!WriteFile(m_pipe, &msg, sizeof(msg), &written, nullptr)) {
        disconnect();
        return 0;
    }
    
    FlushFileBuffers(m_pipe);
    
    IPCResponse resp = {};
    DWORD read = 0;
    
    if (!ReadFile(m_pipe, &resp, sizeof(resp), &read, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            disconnect();
            return 0;
        }
    }
    
    return resp.result;
}

bool IPCClient::readResponse(std::wstring& data) {
    if (!isConnected()) {
        return false;
    }
    
    DWORD read = 0;
    memset(m_buffer.get(), 0, BUFFER_SIZE);
    
    if (!ReadFile(m_pipe, m_buffer.get(), BUFFER_SIZE - 2, &read, nullptr)) {
        if (GetLastError() != ERROR_MORE_DATA) {
            return false;
        }
    }
    
    if (read > 0) {
        data = reinterpret_cast<wchar_t*>(m_buffer.get());
    }
    
    return true;
}

// ============================================
// IPCServer
// ============================================

IPCServer::IPCServer()
    : m_pipe(INVALID_HANDLE_VALUE)
    , m_running(false)
    , m_thread(nullptr) {
    memset(&m_sa, 0, sizeof(m_sa));
}

IPCServer::~IPCServer() {
    stop();
}

bool IPCServer::start() {
    if (m_running) {
        return true;
    }
    
    // 创建安全描述符，允许所有用户连接
    PSECURITY_DESCRIPTOR pSD = nullptr;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GA;;;WD)",
        SDDL_REVISION_1,
        &pSD,
        nullptr
    );
    
    m_sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    m_sa.lpSecurityDescriptor = pSD;
    m_sa.bInheritHandle = FALSE;
    
    m_running = true;
    
    m_thread = CreateThread(
        nullptr,
        0,
        [](LPVOID param) -> DWORD {
            static_cast<IPCServer*>(param)->serverThread();
            return 0;
        },
        this,
        0,
        nullptr
    );
    
    return m_thread != nullptr;
}

void IPCServer::stop() {
    m_running = false;
    
    if (m_pipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
    
    if (m_thread) {
        WaitForSingleObject(m_thread, 3000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    
    if (m_sa.lpSecurityDescriptor) {
        LocalFree(m_sa.lpSecurityDescriptor);
        m_sa.lpSecurityDescriptor = nullptr;
    }
}

void IPCServer::setHandler(RequestHandler handler) {
    m_handler = std::move(handler);
}

void IPCServer::serverThread() {
    std::wstring pipeName = GetPipeName();
    
    while (m_running) {
        m_pipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192,
            8192,
            0,
            &m_sa
        );
        
        if (m_pipe == INVALID_HANDLE_VALUE) {
            Sleep(100);
            continue;
        }
        
        if (ConnectNamedPipe(m_pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            handleClient(m_pipe);
        }
        
        DisconnectNamedPipe(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}

void IPCServer::handleClient(HANDLE pipe) {
    while (m_running) {
        IPCMessage msg = {};
        DWORD read = 0;
        
        if (!ReadFile(pipe, &msg, sizeof(msg), &read, nullptr)) {
            break;
        }
        
        if (read < sizeof(msg)) {
            break;
        }
        
        std::wstring responseData;
        DWORD result = 0;
        
        if (m_handler) {
            result = m_handler(msg, responseData);
        }
        
        IPCResponse resp = { result, static_cast<DWORD>(responseData.size() * sizeof(wchar_t)) };
        DWORD written = 0;
        
        WriteFile(pipe, &resp, sizeof(resp), &written, nullptr);
        
        if (!responseData.empty()) {
            WriteFile(pipe, responseData.c_str(), resp.dataSize, &written, nullptr);
        }
        
        FlushFileBuffers(pipe);
    }
}

} // namespace suyan

#endif // _WIN32
