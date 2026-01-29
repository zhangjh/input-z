#include "server_app.h"
#include "input_engine.h"
#include "candidate_window.h"
#include "theme_manager.h"
#include "rime_wrapper.h"
#include "ipc_protocol.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <fstream>

namespace suyan {

static void ServerLog(const char* msg) {
    std::ofstream f("C:\\temp\\suyan_server.log", std::ios::app);
    if (f.is_open()) {
        f << msg << std::endl;
    }
}

static bool IsIPCServerRunning() {
    HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\SuYanInputMethod", GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        return true;
    }
    return false;
}

ServerApp::ServerApp(QObject* parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr) {
}

ServerApp::~ServerApp() {
    shutdown();
}

bool ServerApp::initialize() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(userDataDir);

    ThemeManager::instance().initialize(appDir + "/themes");

    m_inputEngine = std::make_unique<InputEngine>();
    if (!m_inputEngine->initialize(
            (userDataDir + "/rime").toStdString(),
            (appDir + "/rime").toStdString())) {
        return false;
    }

    m_candidateWindow = std::make_unique<CandidateWindow>();
    m_candidateWindow->connectToThemeManager();

    m_inputEngine->setStateChangedCallback([this](const InputState& state) {
        char buf[128];
        sprintf_s(buf, "StateChanged: isComposing=%d candidates=%zu", state.isComposing ? 1 : 0, state.candidates.size());
        ServerLog(buf);
        if (state.isComposing && !state.candidates.empty()) {
            QMetaObject::invokeMethod(this, [this, state]() {
                m_candidateWindow->updateCandidates(state);
                if (!m_candidateWindow->isWindowVisible()) {
                    int h = m_cursorHeight > 0 ? m_cursorHeight : 20;
                    QRect cursorRect(m_cursorPos.x(), m_cursorPos.y() - h, 1, h);
                    m_candidateWindow->showAtNative(cursorRect);
                }
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(this, [this]() {
                m_candidateWindow->hideWindow();
            }, Qt::QueuedConnection);
        }
    });

    m_inputEngine->setCommitTextCallback([this](const std::string& text) {
        m_commitText = QString::fromStdString(text).toStdWString();
    });

    m_ipcServer = std::make_unique<IPCServer>();
    m_ipcServer->setHandler([this](const IPCMessage& msg, std::wstring& resp) {
        return handleIPCRequest(msg, resp);
    });
    
    if (!m_ipcServer->start()) {
        return false;
    }

    setupTrayIcon();
    return true;
}

void ServerApp::shutdown() {
    if (m_ipcServer) {
        m_ipcServer->stop();
        m_ipcServer.reset();
    }
    if (m_inputEngine) {
        m_inputEngine->shutdown();
    }
    m_inputEngine.reset();
    m_candidateWindow.reset();
}

DWORD ServerApp::handleIPCRequest(const IPCMessage& msg, std::wstring& response) {
    switch (msg.cmd) {
        case IPC_ECHO:
            return msg.sessionId;

        case IPC_START_SESSION:
            if (m_inputEngine) m_inputEngine->activate();
            return 1;

        case IPC_END_SESSION:
            if (m_inputEngine) m_inputEngine->deactivate();
            return 1;

        case IPC_PROCESS_KEY: {
            if (!m_inputEngine) return 0;
            int vk = static_cast<int>(msg.param1);
            int modifiers = static_cast<int>(msg.param2);
            
            int keyCode = convertVirtualKeyToRimeKey(vk, modifiers);
            if (keyCode == 0) return 0;
            
            m_commitText.clear();
            bool handled = m_inputEngine->processKeyEvent(keyCode, modifiers);
            return handled ? 1 : 0;
        }

        case IPC_TEST_KEY: {
            ServerLog("IPC_TEST_KEY received");
            if (!m_inputEngine) {
                ServerLog("IPC_TEST_KEY: m_inputEngine is null");
                return 0;
            }
            int vk = static_cast<int>(msg.param1);
            int modifiers = static_cast<int>(msg.param2);
            
            char buf[128];
            sprintf_s(buf, "IPC_TEST_KEY: vk=%d mod=%d", vk, modifiers);
            ServerLog(buf);
            
            int keyCode = convertVirtualKeyToRimeKey(vk, modifiers);
            if (keyCode == 0) {
                ServerLog("IPC_TEST_KEY: keyCode is 0");
                return 0;
            }
            
            bool shouldEat = shouldProcessKey(keyCode, modifiers);
            sprintf_s(buf, "IPC_TEST_KEY: result=%d", shouldEat ? 1 : 0);
            ServerLog(buf);
            return shouldEat ? 1 : 0;
        }

        case IPC_FOCUS_IN:
            if (m_inputEngine) m_inputEngine->activate();
            return 1;

        case IPC_FOCUS_OUT:
            if (m_inputEngine) m_inputEngine->deactivate();
            if (m_candidateWindow) m_candidateWindow->hideWindow();
            return 1;

        case IPC_UPDATE_POSITION: {
            int16_t x, y;
            unpackCoordinates(msg.param1, x, y);
            int height = static_cast<int>(msg.param2);
            if (height <= 0) height = 20;
            // Update position synchronously to ensure it's available before candidate window shows
            m_cursorPos = QPoint(x, y);
            m_cursorHeight = height;
            QMetaObject::invokeMethod(this, [this, x, y, height]() {
                if (m_candidateWindow && m_candidateWindow->isWindowVisible()) {
                    QRect cursorRect(x, y - height, 1, height);
                    m_candidateWindow->showAtNative(cursorRect);
                }
            }, Qt::QueuedConnection);
            return 1;
        }

        case IPC_COMMIT:
            if (!m_commitText.empty()) {
                response = m_commitText;
                m_commitText.clear();
                return 1;
            }
            return 0;

        case IPC_SHUTDOWN:
            QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
            return 1;

        default:
            return 0;
    }
}

void ServerApp::setupTrayIcon() {
    m_trayMenu = new QMenu();
    
    QAction* settingsAction = m_trayMenu->addAction(QString::fromUtf8("设置(&S)"));
    connect(settingsAction, &QAction::triggered, this, &ServerApp::onSettings);
    
    m_trayMenu->addSeparator();
    
    QAction* quitAction = m_trayMenu->addAction(QString::fromUtf8("退出(&Q)"));
    connect(quitAction, &QAction::triggered, this, &ServerApp::onQuit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(QCoreApplication::applicationDirPath() + "/icons/app-icon.ico"));
    m_trayIcon->setToolTip(QString::fromUtf8("素言输入法"));
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &ServerApp::onTrayActivated);
    m_trayIcon->show();
}

void ServerApp::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    (void)reason;
}

void ServerApp::onSettings() {
    QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString rimeDir = userDataDir + "/rime";
    QDesktopServices::openUrl(QUrl::fromLocalFile(rimeDir));
}

void ServerApp::onQuit() {
    shutdown();
    QCoreApplication::quit();
}

bool ServerApp::shouldProcessKey(int keyCode, int modifiers) {
    if (!m_inputEngine) {
        ServerLog("shouldProcessKey: m_inputEngine is null");
        return false;
    }
    
    bool isComposing = m_inputEngine->isComposing();
    
    char buf[128];
    sprintf_s(buf, "shouldProcessKey: keyCode=%d mod=%d isComposing=%d", keyCode, modifiers, isComposing ? 1 : 0);
    ServerLog(buf);
    
    if (isComposing) {
        return true;
    }
    
    if (modifiers == 0) {
        if (keyCode >= 'a' && keyCode <= 'z') return true;
        if (keyCode >= 'A' && keyCode <= 'Z') return true;
    }
    
    return false;
}

int ServerApp::convertVirtualKeyToRimeKey(int vk, int modifiers) {
    if (vk >= 'A' && vk <= 'Z') {
        bool shift = (modifiers & 1) != 0;
        if (shift) {
            return vk;
        } else {
            return vk + 32;
        }
    }
    
    if (vk >= '0' && vk <= '9') {
        return vk;
    }
    
    switch (vk) {
        case VK_SPACE:   return 0x20;
        case VK_RETURN:  return 0xff0d;
        case VK_BACK:    return 0xff08;
        case VK_ESCAPE:  return 0xff1b;
        case VK_TAB:     return 0xff09;
        case VK_DELETE:  return 0xffff;
        case VK_LEFT:    return 0xff51;
        case VK_UP:      return 0xff52;
        case VK_RIGHT:   return 0xff53;
        case VK_DOWN:    return 0xff54;
        case VK_PRIOR:   return 0xff55;
        case VK_NEXT:    return 0xff56;
        case VK_HOME:    return 0xff50;
        case VK_END:     return 0xff57;
        
        case VK_OEM_1:      return (modifiers & 1) ? ':' : ';';
        case VK_OEM_PLUS:   return (modifiers & 1) ? '+' : '=';
        case VK_OEM_COMMA:  return (modifiers & 1) ? '<' : ',';
        case VK_OEM_MINUS:  return (modifiers & 1) ? '_' : '-';
        case VK_OEM_PERIOD: return (modifiers & 1) ? '>' : '.';
        case VK_OEM_2:      return (modifiers & 1) ? '?' : '/';
        case VK_OEM_3:      return (modifiers & 1) ? '~' : '`';
        case VK_OEM_4:      return (modifiers & 1) ? '{' : '[';
        case VK_OEM_5:      return (modifiers & 1) ? '|' : '\\';
        case VK_OEM_6:      return (modifiers & 1) ? '}' : ']';
        case VK_OEM_7:      return (modifiers & 1) ? '"' : '\'';
    }
    
    return 0;
}

} // namespace suyan
