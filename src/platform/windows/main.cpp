/**
 * Windows 平台主入口
 * Task 8: 主程序入口
 * 
 * DLL 入口点和组件初始化
 * 
 * Windows TSF 输入法作为 DLL 运行，由 TSF 框架加载。
 * 初始化流程：
 * 1. DllMain (DLL_PROCESS_ATTACH) - 保存模块句柄
 * 2. TSFBridge::Activate - 初始化组件（首次激活时）
 * 
 * Requirements: 12.1, 12.2, 12.3, 12.4, 13.1, 13.2, 13.3, 13.4
 */

#ifdef _WIN32

// Qt 头文件必须在 Windows 头文件之前包含
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>

#ifdef Bool
#undef Bool
#endif

#include <windows.h>
#include <shlobj.h>
#include "tsf_bridge.h"
#include "windows_bridge.h"
#include "language_bar.h"
#include "input_engine.h"
#include "rime_wrapper.h"
#include "candidate_window.h"
#include "theme_manager.h"
#include "layout_manager.h"
#include "config_manager.h"
#include "frequency_manager.h"
#include "suyan_ui_init.h"
#include "ipc_channel.h"

#ifdef Bool
#undef Bool
#endif

namespace suyan {

static bool IsIPCServerRunning() {
    HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\SuYanInputMethod", GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
        return true;
    }
    return false;
}

// ============================================
// 全局组件指针
// ============================================

static InputEngine* g_inputEngine = nullptr;
static CandidateWindow* g_candidateWindow = nullptr;
static WindowsBridge* g_windowsBridge = nullptr;
static QApplication* g_qtApp = nullptr;
static IPCServer* g_ipcServer = nullptr;
static bool g_initialized = false;
static CRITICAL_SECTION g_initLock;
static bool g_initLockCreated = false;

// ============================================
// 路径获取函数
// ============================================

/**
 * 获取用户数据目录
 * Windows: %APPDATA%/SuYan/
 * Requirements: 13.1
 */
static QString getUserDataDir() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        QString userDir = QString::fromWCharArray(appDataPath) + "/SuYan";
        QDir dir(userDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        return userDir;
    }
    return QString();
}

/**
 * 获取共享数据目录（RIME 词库）
 * 优先使用 DLL 同目录下的 rime 文件夹
 * Requirements: 13.1
 */
static QString getSharedDataDir() {
    HMODULE hModule = GetModuleHandle();
    if (!hModule) {
        return QString();
    }
    
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(hModule, dllPath, MAX_PATH) == 0) {
        return QString();
    }
    
    QString dllDir = QString::fromWCharArray(dllPath);
    int lastSlash = dllDir.lastIndexOf('\\');
    if (lastSlash > 0) {
        dllDir = dllDir.left(lastSlash);
    }
    
    QString rimePath = dllDir + "/rime";
    if (QDir(rimePath).exists()) {
        return rimePath;
    }
    
    // 开发环境：向上查找 data/rime
    QDir dir(dllDir);
    QStringList searchPaths = {
        "../data/rime",
        "../../data/rime",
        "../../../data/rime",
        "../../../../data/rime"
    };
    
    for (const QString& path : searchPaths) {
        QString fullPath = dir.absoluteFilePath(path);
        if (QDir(fullPath).exists()) {
            return QDir(fullPath).absolutePath();
        }
    }
    
    return QString();
}

/**
 * 获取主题目录
 */
static QString getThemesDir() {
    HMODULE hModule = GetModuleHandle();
    if (!hModule) {
        return QString();
    }
    
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(hModule, dllPath, MAX_PATH) == 0) {
        return QString();
    }
    
    QString dllDir = QString::fromWCharArray(dllPath);
    int lastSlash = dllDir.lastIndexOf('\\');
    if (lastSlash > 0) {
        dllDir = dllDir.left(lastSlash);
    }
    
    QString themesPath = dllDir + "/themes";
    if (QDir(themesPath).exists()) {
        return themesPath;
    }
    
    // 开发环境
    QDir dir(dllDir);
    QStringList searchPaths = {
        "../resources/themes",
        "../../resources/themes",
        "../../../resources/themes",
        "../../../../resources/themes"
    };
    
    for (const QString& path : searchPaths) {
        QString fullPath = dir.absoluteFilePath(path);
        if (QDir(fullPath).exists()) {
            return QDir(fullPath).absolutePath();
        }
    }
    
    return QString();
}

/**
 * 获取图标资源目录
 */
static QString getIconsDir() {
    HMODULE hModule = GetModuleHandle();
    if (!hModule) {
        return QString();
    }
    
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(hModule, dllPath, MAX_PATH) == 0) {
        return QString();
    }
    
    QString dllDir = QString::fromWCharArray(dllPath);
    int lastSlash = dllDir.lastIndexOf('\\');
    if (lastSlash > 0) {
        dllDir = dllDir.left(lastSlash);
    }
    
    QString iconsPath = dllDir + "/icons";
    if (QDir(iconsPath).exists()) {
        return iconsPath;
    }
    
    // 开发环境
    QDir dir(dllDir);
    QStringList searchPaths = {
        "../resources/icons",
        "../../resources/icons",
        "../../../resources/icons",
        "../../../../resources/icons"
    };
    
    for (const QString& path : searchPaths) {
        QString fullPath = dir.absoluteFilePath(path);
        if (QDir(fullPath).exists()) {
            return QDir(fullPath).absolutePath();
        }
    }
    
    return QString();
}

// ============================================
// 初始化函数
// ============================================

/**
 * 获取 DLL 所在目录
 */
static QString getDllDir() {
    HMODULE hModule = GetModuleHandle();
    if (!hModule) {
        return QString();
    }
    
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(hModule, dllPath, MAX_PATH) == 0) {
        return QString();
    }
    
    QString path = QString::fromWCharArray(dllPath);
    int lastSlash = path.lastIndexOf('\\');
    if (lastSlash > 0) {
        return path.left(lastSlash);
    }
    return QString();
}

/**
 * 初始化 Qt 应用
 * Requirements: 12.1, 12.2
 */
static bool initializeQt() {
    if (g_qtApp) {
        return true;
    }
    
    // 设置 Qt 插件路径（必须在创建 QApplication 之前）
    QString dllDir = getDllDir();
    if (!dllDir.isEmpty()) {
        QCoreApplication::addLibraryPath(dllDir);
    }
    
    static int argc = 1;
    static char* argv[] = { const_cast<char*>("SuYan"), nullptr };
    
    g_qtApp = new QApplication(argc, argv);
    g_qtApp->setApplicationName("SuYan");
    g_qtApp->setApplicationVersion("1.0.0");
    g_qtApp->setOrganizationName("zhangjh");
    g_qtApp->setOrganizationDomain("cn.zhangjh");
    g_qtApp->setQuitOnLastWindowClosed(false);
    
    return true;
}

/**
 * 初始化 RIME 引擎
 * Requirements: 13.1
 */
static bool initializeRime() {
    QString userDir = getUserDataDir();
    QString sharedDir = getSharedDataDir();
    
    if (userDir.isEmpty()) {
        qCritical() << "SuYan: Cannot determine user data directory";
        return false;
    }
    
    if (sharedDir.isEmpty()) {
        qCritical() << "SuYan: RIME shared data directory not found";
        return false;
    }
    
    qDebug() << "SuYan: User data dir:" << userDir;
    qDebug() << "SuYan: Shared data dir:" << sharedDir;
    
    auto& rime = RimeWrapper::instance();
    if (!rime.initialize(userDir.toStdString(), sharedDir.toStdString(), "SuYan")) {
        qCritical() << "SuYan: Failed to initialize RIME engine";
        return false;
    }
    
    qDebug() << "SuYan: RIME engine initialized, version:" 
             << QString::fromStdString(rime.getVersion());
    
    return true;
}

/**
 * 初始化 InputEngine
 * Requirements: 13.2, 13.3
 */
static bool initializeInputEngine() {
    QString userDir = getUserDataDir();
    QString sharedDir = getSharedDataDir();
    
    // 初始化 FrequencyManager
    auto& freqMgr = FrequencyManager::instance();
    if (!freqMgr.isInitialized()) {
        if (freqMgr.initialize(userDir.toStdString())) {
            qDebug() << "SuYan: FrequencyManager initialized";
        } else {
            qWarning() << "SuYan: Failed to initialize FrequencyManager";
        }
    }
    
    g_inputEngine = new InputEngine();
    
    if (!g_inputEngine->initialize(userDir.toStdString(), sharedDir.toStdString())) {
        qCritical() << "SuYan: Failed to initialize InputEngine";
        delete g_inputEngine;
        g_inputEngine = nullptr;
        return false;
    }
    
    // 创建 WindowsBridge
    g_windowsBridge = new WindowsBridge();
    g_inputEngine->setPlatformBridge(g_windowsBridge);
    
    qDebug() << "SuYan: InputEngine initialized";
    return true;
}

/**
 * 初始化 UI 组件
 * Requirements: 13.3
 */
static bool initializeUIComponents() {
    QString userDir = getUserDataDir();
    
    // 初始化 ConfigManager
    auto& configMgr = ConfigManager::instance();
    if (!configMgr.isInitialized()) {
        configMgr.initialize(userDir.toStdString());
    }
    
    // 使用 UI 初始化器
    UIInitConfig config;
    config.themesDir = getThemesDir();
    config.followSystemTheme = true;
    
    UIInitResult result = suyan::initializeUI(config);
    
    if (!result.success) {
        qCritical() << "SuYan: Failed to initialize UI:" << result.errorMessage;
        return false;
    }
    
    g_candidateWindow = result.window;
    
    qDebug() << "SuYan: UI initialized";
    return true;
}

/**
 * 连接组件
 * Requirements: 12.4
 * 
 * 注意：TSF 回调可能在不同线程中执行，必须使用 QMetaObject::invokeMethod
 * 确保 Qt UI 操作在主线程执行，否则会导致跨线程访问错误。
 */
static void connectComponents() {
    if (!g_inputEngine || !g_candidateWindow || !g_windowsBridge) {
        return;
    }
    
    // 状态变化回调
    g_inputEngine->setStateChangedCallback([](const InputState& state) {
        if (g_candidateWindow) {
            InputState stateCopy = state;
            
            QMetaObject::invokeMethod(g_candidateWindow, [stateCopy]() {
                if (g_candidateWindow) {
                    g_candidateWindow->updateCandidates(stateCopy);
                    
                    if (!stateCopy.isComposing || stateCopy.candidates.empty()) {
                        g_candidateWindow->hideWindow();
                    }
                }
            }, Qt::QueuedConnection);
            
            if (QCoreApplication::instance()) {
                QCoreApplication::processEvents();
            }
        }
        
        // 更新 LanguageBar 图标
        auto& langBar = LanguageBar::instance();
        if (langBar.isInitialized()) {
            langBar.updateIcon(state.mode);
        }
    });
    
    // 文字提交回调 - 这个在 TSF 线程中执行是正确的
    g_inputEngine->setCommitTextCallback([](const std::string& text) {
        if (g_windowsBridge) {
            g_windowsBridge->commitText(text);
        }
    });
    
    qDebug() << "SuYan: Components connected";
}

// ============================================
// 公开接口
// ============================================

/**
 * 初始化所有组件
 * 在 TSFBridge::Activate 首次调用时执行
 * Requirements: 12.1, 12.2, 13.1, 13.2, 13.3
 */
bool InitializeComponents() {
    if (g_initialized) {
        return true;
    }
    
    EnterCriticalSection(&g_initLock);
    
    if (g_initialized) {
        LeaveCriticalSection(&g_initLock);
        return true;
    }
    
    qDebug() << "SuYan: Initializing components...";
    
    // 1. 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qCritical() << "SuYan: Failed to initialize COM";
        LeaveCriticalSection(&g_initLock);
        return false;
    }
    
    // 2. 初始化 Qt
    if (!initializeQt()) {
        qCritical() << "SuYan: Failed to initialize Qt";
        LeaveCriticalSection(&g_initLock);
        return false;
    }
    
    // 3. 初始化 RIME
    if (!initializeRime()) {
        ShowErrorDialog(L"RIME 引擎初始化失败", 
                        L"请检查 RIME 数据文件是否完整。");
        LeaveCriticalSection(&g_initLock);
        return false;
    }
    
    // 4. 初始化 InputEngine
    if (!initializeInputEngine()) {
        ShowErrorDialog(L"输入引擎初始化失败", 
                        L"请检查配置文件是否正确。");
        LeaveCriticalSection(&g_initLock);
        return false;
    }
    
    // 5. 初始化 UI
    if (!initializeUIComponents()) {
        ShowErrorDialog(L"UI 初始化失败", 
                        L"请检查主题文件是否存在。");
        LeaveCriticalSection(&g_initLock);
        return false;
    }
    
    // 6. 连接组件（LanguageBar 在 TSFBridge::Activate 中初始化）
    connectComponents();
    
    // 7. 启动IPC服务端（为32位客户端提供服务）
    if (!IsIPCServerRunning()) {
        g_ipcServer = new IPCServer();
        g_ipcServer->setHandler([](const IPCMessage& msg, std::wstring& response) -> DWORD {
            if (!g_inputEngine) return 0;
            
            switch (msg.cmd) {
                case IPC_ECHO:
                    return msg.sessionId;
                    
                case IPC_START_SESSION:
                    return 1;
                    
                case IPC_END_SESSION:
                    return 1;
                    
                case IPC_PROCESS_KEY: {
                    int keyCode = static_cast<int>(msg.param1);
                    int modifiers = static_cast<int>(msg.param2);
                    bool handled = g_inputEngine->processKeyEvent(keyCode, modifiers);
                    return handled ? 1 : 0;
                }
                    
                case IPC_FOCUS_IN:
                    g_inputEngine->activate();
                    return 1;
                    
                case IPC_FOCUS_OUT:
                    g_inputEngine->deactivate();
                    return 1;
                    
                case IPC_UPDATE_POSITION: {
                    DWORD compressed = msg.param1;
                    int left = (compressed & 0xfff);
                    if (left & 0x800) left |= 0xFFFFF000;
                    int top = ((compressed >> 12) & 0xfff);
                    if (top & 0x800) top |= 0xFFFFF000;
                    int height = (compressed >> 24) & 0x7f;
                    (void)left; (void)top; (void)height;
                    return 1;
                }
                    
                case IPC_COMMIT:
                    return 1;
                    
                default:
                    return 0;
            }
        });
        g_ipcServer->start();
        qDebug() << "SuYan: IPC server started";
    } else {
        qDebug() << "SuYan: IPC server already running, skipping";
    }
    
    g_initialized = true;
    qDebug() << "SuYan: All components initialized successfully";
    
    LeaveCriticalSection(&g_initLock);
    return true;
}

/**
 * 清理所有组件
 * Requirements: 12.3
 */
void CleanupComponents() {
    if (!g_initialized) {
        return;
    }
    
    EnterCriticalSection(&g_initLock);
    
    qDebug() << "SuYan: Cleaning up components...";
    
    // 清理 IPC 服务端
    if (g_ipcServer) {
        g_ipcServer->stop();
        delete g_ipcServer;
        g_ipcServer = nullptr;
    }
    
    // 清理 LanguageBar
    auto& langBar = LanguageBar::instance();
    if (langBar.isInitialized()) {
        langBar.shutdown();
    }
    
    // 清理 UI
    if (g_candidateWindow) {
        cleanupUI(g_candidateWindow);
        g_candidateWindow = nullptr;
    }
    
    // 清理 InputEngine
    if (g_inputEngine) {
        g_inputEngine->shutdown();
        delete g_inputEngine;
        g_inputEngine = nullptr;
    }
    
    // 清理 WindowsBridge
    if (g_windowsBridge) {
        delete g_windowsBridge;
        g_windowsBridge = nullptr;
    }
    
    // 清理 FrequencyManager
    FrequencyManager::instance().shutdown();
    
    // 清理 RIME
    RimeWrapper::instance().finalize();
    
    // 清理 Qt
    if (g_qtApp) {
        delete g_qtApp;
        g_qtApp = nullptr;
    }
    
    g_initialized = false;
    qDebug() << "SuYan: Cleanup complete";
    
    LeaveCriticalSection(&g_initLock);
}

/**
 * 显示错误对话框
 * Requirements: 13.4
 */
void ShowErrorDialog(const wchar_t* title, const wchar_t* message) {
    MessageBoxW(nullptr, message, title, MB_OK | MB_ICONERROR);
}

/**
 * 获取 InputEngine 实例
 */
InputEngine* GetInputEngine() {
    return g_inputEngine;
}

/**
 * 获取 CandidateWindow 实例
 */
CandidateWindow* GetCandidateWindow() {
    return g_candidateWindow;
}

/**
 * 获取 WindowsBridge 实例
 */
WindowsBridge* GetWindowsBridge() {
    return g_windowsBridge;
}

/**
 * 检查是否已初始化
 */
bool IsInitialized() {
    return g_initialized;
}

} // namespace suyan

// ============================================
// DLL 入口点
// ============================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            suyan::SetModuleHandle(hinstDLL);
            DisableThreadLibraryCalls(hinstDLL);
            
            // 创建初始化锁
            if (!suyan::g_initLockCreated) {
                InitializeCriticalSection(&suyan::g_initLock);
                suyan::g_initLockCreated = true;
            }
            break;
            
        case DLL_PROCESS_DETACH:
            // 清理组件
            suyan::CleanupComponents();
            
            // 删除初始化锁
            if (suyan::g_initLockCreated) {
                DeleteCriticalSection(&suyan::g_initLock);
                suyan::g_initLockCreated = false;
            }
            break;
    }
    
    return TRUE;
}

#endif // _WIN32
