/**
 * TSFBridge 实现
 * Task 4: TSFBridge COM 基础实现
 * Task 5: TSFBridge 核心接口实现
 * 
 * 实现 TSF COM 组件的核心功能
 */

#ifdef _WIN32

// Qt 头文件必须在任何可能定义 Bool 宏的头文件之前包含
// rime_api.h 和 Windows 头文件都定义了 Bool 宏，与 Qt 的 qmetatype.h 冲突
// 因此必须在最开始包含 Qt 头文件
#include <QtCore/QtCore>
#include <QPoint>

#include "tsf_bridge.h"
#include "windows_bridge.h"
#include "language_bar.h"
#include "key_converter.h"
#include "input_engine.h"
#include "candidate_window.h"
#include <ctffunc.h>
#include <QMetaObject>
#include <QScreen>
#include <QGuiApplication>
#include <strsafe.h>
#include <imm.h>
#include <fstream>
#include <sstream>
#pragma comment(lib, "imm32.lib")

// 调试日志函数
// 调试日志函数
static void DebugLog(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // 1. 输出到调试器 (DebugView) - 适用于所有环境
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
    
    // 2. 输出到文件 (尝试 LocalLow 以兼容沙盒)
    static std::ofstream logFile;
    static bool initialized = false;
    
    if (!initialized) {
        // 使用 PID 避免多进程冲突
        DWORD pid = GetCurrentProcessId();
        char path[MAX_PATH];
        
        // 尝试 LocalLow (Electron/Chrome 沙盒通常允许写入此处)
        // C:\Users\<User>\AppData\LocalLow\suyan_debug_<PID>.log
        // 暂时硬编码用户路径以确保测试准确 (User: cn-dantezhang)
        sprintf_s(path, "C:\\Users\\cn-dantezhang\\AppData\\LocalLow\\suyan_debug_%lu.log", pid);
        
        logFile.open(path, std::ios::app);
        if (!logFile.is_open()) {
            // 回退到 Temp
            GetTempPathA(MAX_PATH, path);
            char filename[64];
            sprintf_s(filename, "suyan_ime_debug_%lu.log", pid);
            strcat_s(path, filename);
            logFile.open(path, std::ios::app);
        }
        initialized = true;
    }
    
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        logFile << "[" << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "." << st.wMilliseconds << "] "
                << buffer << std::endl;
        logFile.flush();
    }
}

namespace suyan {


// ============================================
// 全局变量
// ============================================

// DLL 模块句柄
static HMODULE g_hModule = nullptr;

// DLL 引用计数（用于 DllCanUnloadNow）
static LONG g_dllRefCount = 0;

// 服务器锁定计数
static LONG g_serverLockCount = 0;

// 获取 DLL 模块句柄
HMODULE GetModuleHandle() {
    return g_hModule;
}

// 设置 DLL 模块句柄（在 DllMain 中调用）
void SetModuleHandle(HMODULE hModule) {
    g_hModule = hModule;
}

// 增加 DLL 引用计数
void DllAddRef() {
    InterlockedIncrement(&g_dllRefCount);
}

// 减少 DLL 引用计数
void DllRelease() {
    InterlockedDecrement(&g_dllRefCount);
}

// ============================================
// CLSID 和 GUID 定义
// ============================================

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
const CLSID CLSID_SuYanTextService = {
    0xA1B2C3D4, 0xE5F6, 0x7890,
    { 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90 }
};

// {B2C3D4E5-F6A7-8901-BCDE-F12345678901}
const GUID GUID_SuYanProfile = {
    0xB2C3D4E5, 0xF6A7, 0x8901,
    { 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x01 }
};

// ============================================
// GUID 字符串转换辅助函数
// ============================================

// 将 GUID 转换为注册表格式字符串 {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
static bool GuidToString(const GUID& guid, wchar_t* buffer, size_t bufferSize) {
    return SUCCEEDED(StringCchPrintfW(buffer, bufferSize,
        L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]));
}

// ============================================
// 注册表操作辅助函数
// ============================================

// 创建注册表键并设置默认值
static HRESULT CreateRegKeyAndSetValue(HKEY hKeyRoot, const wchar_t* subKey, 
                                        const wchar_t* valueName, const wchar_t* value) {
    HKEY hKey = nullptr;
    LONG result = RegCreateKeyExW(hKeyRoot, subKey, 0, nullptr, 
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (result != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(result);
    }

    if (value) {
        result = RegSetValueExW(hKey, valueName, 0, REG_SZ, 
                                 reinterpret_cast<const BYTE*>(value),
                                 static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    }

    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(result);
}

// 递归删除注册表键
static HRESULT DeleteRegKey(HKEY hKeyRoot, const wchar_t* subKey) {
    // 使用 RegDeleteTreeW 递归删除（Windows Vista+）
    LONG result = RegDeleteTreeW(hKeyRoot, subKey);
    if (result == ERROR_FILE_NOT_FOUND) {
        return S_OK; // 键不存在，视为成功
    }
    return HRESULT_FROM_WIN32(result);
}

// ============================================
// GetTextExtEditSession 实现
// ============================================

GetTextExtEditSession::GetTextExtEditSession(ITfContext* pContext, ITfComposition* pComposition, TSFBridge* pBridge)
    : context_(pContext)
    , composition_(pComposition)
    , bridge_(pBridge)
    , textRect_{}
    , isValid_(false) {
    if (context_) context_->AddRef();
    if (composition_) composition_->AddRef();
}

GetTextExtEditSession::~GetTextExtEditSession() {
    if (context_) context_->Release();
    if (composition_) composition_->Release();
}

STDMETHODIMP GetTextExtEditSession::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) GetTextExtEditSession::AddRef() {
    return ++refCount_;
}

STDMETHODIMP_(ULONG) GetTextExtEditSession::Release() {
    ULONG count = --refCount_;
    if (count == 0) delete this;
    return count;
}

STDMETHODIMP GetTextExtEditSession::DoEditSession(TfEditCookie ec) {
    isValid_ = false;
    
    if (!context_ || !bridge_) return E_FAIL;
    
    ITfContextView* contextView = nullptr;
    HRESULT hr = context_->GetActiveView(&contextView);
    if (FAILED(hr) || !contextView) return hr;
    
    ITfRange* range = nullptr;
    
    // 优先使用 composition 的范围
    if (composition_) {
        hr = composition_->GetRange(&range);
        if (SUCCEEDED(hr) && range) {
            range->Collapse(ec, TF_ANCHOR_START);
        }
    }
    
    // 如果没有 composition 或获取失败，尝试获取当前选择
    if (!range) {
        TF_SELECTION selection;
        ULONG fetched = 0;
        hr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (SUCCEEDED(hr) && fetched > 0 && selection.range) {
            range = selection.range;
        }
    }
    
    if (!range) {
        contextView->Release();
        return E_FAIL;
    }
    
    BOOL clipped = FALSE;
    hr = contextView->GetTextExt(ec, range, &textRect_, &clipped);
    
    DebugLog("GetTextExtEditSession::DoEditSession - GetTextExt result: hr=0x%08X, rect=(%d,%d,%d,%d), clipped=%d",
             hr, textRect_.left, textRect_.top, textRect_.right, textRect_.bottom, clipped);
    
    range->Release();
    contextView->Release();
    
    // 验证 GetTextExt 结果
    bool useTextExt = false;
    if (SUCCEEDED(hr) && (textRect_.left != 0 || textRect_.top != 0 || 
                          textRect_.right != 0 || textRect_.bottom != 0)) {
        // 检查是否在当前前台窗口范围内
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            
            // 简单的边界检查：如果 TextExt 完全在窗口外部，可能是不正确的
            // 注意：多显示器情况下，窗口可能在负坐标，所以要小心判断
            // 这里主要防范 (0,0) 这种无效坐标，或者明显错误的坐标
            
            // Weasel 策略：如果坐标异常，尝试使用 GetCaretPos 进行修正或回退
            if (textRect_.left >= rcWindow.left && textRect_.right <= rcWindow.right &&
                textRect_.top >= rcWindow.top && textRect_.bottom <= rcWindow.bottom) {
                useTextExt = true;
            } else {
                // 如果在窗口外，但坐标不是 (0,0)，可能是正常的（比如悬浮窗）
                // 只有当坐标极度不合理时才舍弃
                if (abs(textRect_.left) < 10000 && abs(textRect_.top) < 10000) {
                     useTextExt = true;
                }
            }
        } else {
            useTextExt = true;
        }
    }
    
    if (useTextExt) {
        isValid_ = true;
        DebugLog("  -> Using GetTextExt position: (%d, %d)", textRect_.left, textRect_.top);
        bridge_->setCompositionPosition(textRect_);
        return hr;
    }
    
    // Fallback: 使用 GetCaretPos
    DebugLog("  -> GetTextExt failed or invalid, trying GetCaretPos");
    
    RECT bestRect = textRect_;
    bool positionFixed = false;
    
    POINT caretPt;
    if (::GetCaretPos(&caretPt)) {
        HWND focusWnd = GetFocus();
        if (focusWnd) {
            ::ClientToScreen(focusWnd, &caretPt);
            bestRect.left = caretPt.x;
            bestRect.top = caretPt.y;
            bestRect.right = caretPt.x + 2;
            bestRect.bottom = caretPt.y + 20;  // 默认行高
            positionFixed = true;
            DebugLog("  -> Using GetCaretPos position: (%d, %d)", bestRect.left, bestRect.top);
        }
    }
    
    // 如果 GetCaretPos 也失败，尝试 GetGUIThreadInfo
    if (!positionFixed) {
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
            if (threadId != 0) {
                GUITHREADINFO gti;
                gti.cbSize = sizeof(GUITHREADINFO);
                if (GetGUIThreadInfo(threadId, &gti)) {
                    if (gti.rcCaret.left != 0 || gti.rcCaret.top != 0) {
                        HWND caretWnd = gti.hwndCaret ? gti.hwndCaret : 
                                       (gti.hwndFocus ? gti.hwndFocus : hwnd);
                        POINT topLeft = { gti.rcCaret.left, gti.rcCaret.top };
                        ClientToScreen(caretWnd, &topLeft);
                        bestRect.left = topLeft.x;
                        bestRect.top = topLeft.y;
                        bestRect.right = topLeft.x + (gti.rcCaret.right - gti.rcCaret.left);
                        if (bestRect.right <= bestRect.left) bestRect.right = bestRect.left + 2;
                        bestRect.bottom = topLeft.y + (gti.rcCaret.bottom - gti.rcCaret.top);
                        if (bestRect.bottom <= bestRect.top) bestRect.bottom = bestRect.top + 20;
                        
                        positionFixed = true;
                        DebugLog("  -> Using GetGUIThreadInfo position: (%d, %d)", bestRect.left, bestRect.top);
                    }
                }
            }
        }
    }
    
    if (positionFixed) {
        isValid_ = true;
        bridge_->setCompositionPosition(bestRect);
    }
    
    return hr;
}



// ============================================
// TSFBridge 实现
// ============================================

TSFBridge::TSFBridge() = default;

TSFBridge::~TSFBridge() {
    Deactivate();
}

// IUnknown
STDMETHODIMP TSFBridge::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
    } else if (IsEqualIID(riid, IID_ITfTextLayoutSink)) {
        *ppvObj = static_cast<ITfTextLayoutSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfTextEditSink)) {
        *ppvObj = static_cast<ITfTextEditSink*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TSFBridge::AddRef() {
    return ++refCount_;
}

STDMETHODIMP_(ULONG) TSFBridge::Release() {
    ULONG count = --refCount_;
    if (count == 0) {
        delete this;
    }
    return count;
}

// 前向声明初始化函数
bool InitializeComponents();
InputEngine* GetInputEngine();
CandidateWindow* GetCandidateWindow();
WindowsBridge* GetWindowsBridge();

// ITfTextInputProcessor
/**
 * Activate - 激活输入法
 * Task 5.1: 实现 ITfTextInputProcessor
 * Task 8: 主程序入口 - 组件初始化
 * 
 * 当用户选择此输入法时由 TSF 调用。
 * 职责：
 * 1. 初始化所有组件（首次激活时）
 * 2. 保存 ITfThreadMgr 和 TfClientId
 * 3. 初始化键盘事件接收器
 * 4. 激活 InputEngine
 * 
 * Requirements: 1.1, 1.5, 12.1, 12.2, 13.1, 13.2, 13.3
 */
STDMETHODIMP TSFBridge::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    DebugLog("TSFBridge::Activate called. ThreadMgr=%p, ClientId=%d", pThreadMgr, tfClientId);
    if (!pThreadMgr) {
        DebugLog("  -> pThreadMgr is NULL");
        return E_INVALIDARG;
    }
    
    // Task 8: 首次激活时初始化所有组件
    if (!InitializeComponents()) {
        DebugLog("  -> InitializeComponents failed");
        return E_FAIL;
    }
    DebugLog("  -> InitializeComponents success");
    
    // 获取组件引用
    inputEngine_ = GetInputEngine();
    candidateWindow_ = GetCandidateWindow();
    windowsBridge_ = GetWindowsBridge();
    
    // 保存 TSF 对象
    threadMgr_ = pThreadMgr;
    threadMgr_->AddRef();
    clientId_ = tfClientId;
    
    // 初始化键盘事件接收器
    HRESULT hr = initKeySink();
    if (FAILED(hr)) {
        DebugLog("  -> initKeySink failed: 0x%x", hr);
        threadMgr_->Release();
        threadMgr_ = nullptr;
        clientId_ = TF_CLIENTID_NULL;
        return hr;
    }

    // 初始化 ThreadMgrEventSink (关键修复)
    if (FAILED(initThreadMgrSink())) {
        DebugLog("  -> initThreadMgrSink failed");
        uninitKeySink();
        threadMgr_->Release();
        threadMgr_ = nullptr;
        clientId_ = TF_CLIENTID_NULL;
        return E_FAIL;
    }

    // 如果当前已有焦点文档，立即初始化 TextEditSink
    ITfDocumentMgr* pDocMgrFocus = nullptr;
    if (SUCCEEDED(threadMgr_->GetFocus(&pDocMgrFocus)) && pDocMgrFocus) {
        DebugLog("  -> Found existing focus doc, initializing TextEditSink");
        _InitTextEditSink(pDocMgrFocus);
        pDocMgrFocus->Release();
    } else {
        DebugLog("  -> No existing focus doc");
    }
    
    // 初始化 LanguageBar
    auto& langBar = LanguageBar::instance();
    if (!langBar.isInitialized()) {
        langBar.initialize(threadMgr_);
    }
    
    // 激活 InputEngine
    if (inputEngine_) {
        inputEngine_->activate();
    }
    
    // 更新 WindowsBridge 的 TSFBridge 引用
    if (windowsBridge_) {
        windowsBridge_->setTSFBridge(this);
    }
    
    activated_ = true;
    
    DebugLog("TSFBridge::Activate success");
    return S_OK;
}

/**
 * Deactivate - 停用输入法
 * Task 5.1: 实现 ITfTextInputProcessor
 * 
 * 当用户切换到其他输入法时由 TSF 调用。
 * 职责：
 * 1. 清理键盘事件接收器
 * 2. 结束当前 composition
 * 3. 释放 TSF 资源
 * 4. 停用 InputEngine
 * 
 * Requirements: 1.1, 1.5
 */
STDMETHODIMP TSFBridge::Deactivate() {
    activated_ = false;
    
    // 结束当前 composition
    if (composition_) {
        endComposition();
    }
    
    // 清理 TextEditSink (包含 TextLayoutSink)
    _InitTextEditSink(nullptr);
    
    // 清理 ThreadMgrEventSink
    uninitThreadMgrSink();
    
    // 清理键盘事件接收器
    uninitKeySink();
    
    // 停用 InputEngine
    if (inputEngine_) {
        inputEngine_->deactivate();
    }
    
    // 隐藏候选词窗口
    if (candidateWindow_) {
        QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
        if (QCoreApplication::instance()) {
            QCoreApplication::processEvents();
        }
    }
    
    // 释放 TSF 资源
    if (threadMgr_) {
        threadMgr_->Release();
        threadMgr_ = nullptr;
    }
    clientId_ = TF_CLIENTID_NULL;
    currentContext_ = nullptr;
    
    return S_OK;
}

// ITfKeyEventSink
/**
 * OnSetFocus - 处理焦点变化
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * 当输入焦点变化时由 TSF 调用。
 * 
 * @param fForeground TRUE 表示获得焦点，FALSE 表示失去焦点
 * Requirements: 1.2
 */
STDMETHODIMP TSFBridge::OnSetFocus(BOOL fForeground) {
    DebugLog("TSFBridge::OnSetFocus(%d)", fForeground);
    if (fForeground) {
        // 获得焦点
        // 重置 Shift 键状态
        shiftKeyPressed_ = false;
        otherKeyPressedWithShift_ = false;
    } else {
        // 失去焦点
        if (candidateWindow_) {
            QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
            if (QCoreApplication::instance()) {
                QCoreApplication::processEvents();
            }
        }
    }
    
    return S_OK;
}

/**
 * OnTestKeyDown - 预测试按键按下
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * TSF 在实际处理按键前调用此方法，询问输入法是否要处理此按键。
 * 返回 pfEaten=TRUE 表示输入法将处理此按键。
 * 
 * @param pContext 当前上下文
 * @param wParam 虚拟键码
 * @param lParam 按键信息
 * @param pfEaten 输出：是否消费此按键
 * Requirements: 2.3
 */
STDMETHODIMP TSFBridge::OnTestKeyDown(ITfContext* pContext, WPARAM wParam, 
                                       LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) {
        return E_INVALIDARG;
    }
    
    *pfEaten = FALSE;
    
    if (!inputEngine_ || !activated_) {
        return S_OK;
    }
    
    // 检查是否为修饰键（修饰键不消费）
    if (isModifierKey(wParam)) {
        return S_OK;
    }
    
    // 英文模式下不消费任何按键
    if (inputEngine_->getMode() == InputMode::English) {
        return S_OK;
    }
    
    // 中文模式下，检查是否应该消费此按键
    // 如果正在输入（有 preedit），消费大部分按键
    if (inputEngine_->isComposing()) {
        // 正在输入时，消费字母、数字、标点、功能键等
        if (isCharacterKey(wParam) || isNavigationKey(wParam) ||
            wParam == VK_BACK || wParam == VK_ESCAPE || 
            wParam == VK_RETURN || wParam == VK_SPACE ||
            wParam == VK_PRIOR || wParam == VK_NEXT) {
            *pfEaten = TRUE;
        }
    } else {
        // 未输入时，只消费字母键（开始新的输入）
        if (wParam >= 'A' && wParam <= 'Z') {
            *pfEaten = TRUE;
        }
    }
    
    return S_OK;
}

/**
 * OnTestKeyUp - 预测试按键释放
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * @param pContext 当前上下文
 * @param wParam 虚拟键码
 * @param lParam 按键信息
 * @param pfEaten 输出：是否消费此按键
 * Requirements: 2.7
 */
STDMETHODIMP TSFBridge::OnTestKeyUp(ITfContext* pContext, WPARAM wParam, 
                                     LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) {
        return E_INVALIDARG;
    }
    
    *pfEaten = FALSE;
    
    // Shift 键释放需要特殊处理（用于切换模式）
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        if (shiftKeyPressed_ && !otherKeyPressedWithShift_) {
            // 可能需要切换模式，消费此事件
            *pfEaten = TRUE;
        }
    }
    
    return S_OK;
}

/**
 * OnKeyDown - 处理按键按下
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * 实际处理按键事件的核心方法。
 * 职责：
 * 1. 转换键码
 * 2. 调用 InputEngine::processKeyEvent
 * 3. 根据返回值设置 pfEaten
 * 4. 更新候选词窗口位置
 * 
 * @param pContext 当前上下文
 * @param wParam 虚拟键码
 * @param lParam 按键信息
 * @param pfEaten 输出：是否消费此按键
 * Requirements: 2.3, 2.4, 2.5, 2.6
 */
STDMETHODIMP TSFBridge::OnKeyDown(ITfContext* pContext, WPARAM wParam, 
                                   LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) {
        return E_INVALIDARG;
    }
    
    *pfEaten = FALSE;
    
    if (!inputEngine_ || !activated_) {
        return S_OK;
    }
    
    // 更新当前上下文
    if (currentContext_ != pContext) {
        currentContext_ = pContext;
    }
    if (windowsBridge_) {
        windowsBridge_->setContext(pContext);
    }
    
    // 处理 Shift 键状态跟踪
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        shiftKeyPressed_ = true;
        otherKeyPressedWithShift_ = false;
        shiftPressTime_ = GetTickCount();
        return S_OK;  // Shift 键本身不消费
    }
    
    // 如果 Shift 按下期间有其他键按下，标记
    if (shiftKeyPressed_) {
        otherKeyPressedWithShift_ = true;
    }
    
    // 检查是否为其他修饰键（不消费）
    if (isModifierKey(wParam)) {
        return S_OK;
    }
    
    // 转换键码
    int rimeKeyCode = convertVirtualKeyToRime(wParam, lParam);
    int rimeModifiers = convertModifiers();
    
    if (rimeKeyCode == 0) {
        return S_OK;
    }
    
    // 调用 InputEngine 处理按键
    // Requirements: 2.4, 2.5 - 根据 InputEngine 返回值设置 pfEaten
    bool handled = inputEngine_->processKeyEvent(rimeKeyCode, rimeModifiers);
    *pfEaten = handled ? TRUE : FALSE;
    
    // 如果按键被处理，更新候选词窗口位置
    if (handled) {
        updateCandidateWindowPosition();
    }
    
    return S_OK;
}

/**
 * OnKeyUp - 处理按键释放
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * 主要用于处理 Shift 键释放切换模式。
 * 
 * @param pContext 当前上下文
 * @param wParam 虚拟键码
 * @param lParam 按键信息
 * @param pfEaten 输出：是否消费此按键
 * Requirements: 2.7
 */
STDMETHODIMP TSFBridge::OnKeyUp(ITfContext* pContext, WPARAM wParam, 
                                 LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) {
        return E_INVALIDARG;
    }
    
    *pfEaten = FALSE;
    
    if (!inputEngine_ || !activated_) {
        return S_OK;
    }
    
    // 处理 Shift 键释放切换模式
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        if (shiftKeyPressed_ && !otherKeyPressedWithShift_) {
            // 检查按下时间（避免长按）
            DWORD elapsed = GetTickCount() - shiftPressTime_;
            if (elapsed < 500) {  // 500ms 内释放
                handleShiftKeyRelease();
                *pfEaten = TRUE;
            }
        }
        shiftKeyPressed_ = false;
        otherKeyPressedWithShift_ = false;
    }
    
    return S_OK;
}

/**
 * OnPreservedKey - 处理保留键
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * 处理通过 ITfKeystrokeMgr::PreserveKey 注册的保留键。
 * 当前实现不使用保留键。
 * 
 * @param pContext 当前上下文
 * @param rguid 保留键 GUID
 * @param pfEaten 输出：是否消费此按键
 */
STDMETHODIMP TSFBridge::OnPreservedKey(ITfContext* pContext, REFGUID rguid, 
                                        BOOL* pfEaten) {
    if (pfEaten) {
        *pfEaten = FALSE;
    }
    return S_OK;
}

// ITfCompositionSink
/**
 * OnCompositionTerminated - 处理 composition 被外部终止
 * Task 5.4: 实现 ITfCompositionSink
 * 
 * 当 composition 被应用程序或其他组件终止时由 TSF 调用。
 * 需要清理内部状态。
 * 
 * @param ecWrite 编辑 cookie
 * @param pComposition 被终止的 composition
 * Requirements: 1.3, 3.3
 */
STDMETHODIMP TSFBridge::OnCompositionTerminated(TfEditCookie ecWrite, 
                                                 ITfComposition* pComposition) {
    // 检查是否是我们的 composition
    if (pComposition == composition_) {
        // 释放 composition 引用
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        
        // 重置 InputEngine 状态
        if (inputEngine_) {
            inputEngine_->reset();
        }
        
        // 隐藏候选词窗口
        if (candidateWindow_) {
            QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
            if (QCoreApplication::instance()) {
                QCoreApplication::processEvents();
            }
        }
    }
    
    return S_OK;
}

// ITfDisplayAttributeProvider
/**
 * EnumDisplayAttributeInfo - 枚举显示属性
 * Task 5.5: 实现 ITfDisplayAttributeProvider（可选）
 * 
 * 返回输入法支持的显示属性枚举器。
 * 当前实现返回 E_NOTIMPL，使用系统默认样式。
 * 
 * @param ppEnum 输出：显示属性枚举器
 * Requirements: 4.1
 */
STDMETHODIMP TSFBridge::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    // 可选实现：返回 E_NOTIMPL 使用系统默认样式
    // 如果需要自定义 preedit 下划线样式，可以在这里实现
    if (ppEnum) {
        *ppEnum = nullptr;
    }
    return E_NOTIMPL;
}

/**
 * GetDisplayAttributeInfo - 获取指定的显示属性
 * Task 5.5: 实现 ITfDisplayAttributeProvider（可选）
 * 
 * 根据 GUID 返回对应的显示属性信息。
 * 当前实现返回 E_NOTIMPL，使用系统默认样式。
 * 
 * @param guid 显示属性 GUID
 * @param ppInfo 输出：显示属性信息
 * Requirements: 4.1
 */
STDMETHODIMP TSFBridge::GetDisplayAttributeInfo(REFGUID guid, 
                                                 ITfDisplayAttributeInfo** ppInfo) {
    // 可选实现：返回 E_NOTIMPL 使用系统默认样式
    if (ppInfo) {
        *ppInfo = nullptr;
    }
    return E_NOTIMPL;
}

// ITfTextLayoutSink
/**
 * OnLayoutChange - 处理文本布局变化
 * 
 * 当文本布局发生变化时由 TSF 调用。
 * 用于更新候选词窗口位置。
 * 
 * @param pContext TSF 上下文
 * @param lcode 布局变化类型
 * @param pView 上下文视图
 */
STDMETHODIMP TSFBridge::OnLayoutChange(ITfContext* pContext, TfLayoutCode lcode, 
                                        ITfContextView* pView) {
    (void)pContext;
    (void)pView;
    
    if (lcode == TF_LC_CHANGE || lcode == TF_LC_CREATE) {
        updateCandidateWindowPosition();
    }
    
    return S_OK;
}

// 输入组合管理
/**
 * startComposition - 开始新的输入组合
 * Task 5.4: 实现 ITfCompositionSink
 * 
 * 创建新的 TSF composition，用于管理 preedit 文本。
 * 
 * @param pContext TSF 上下文
 * @return HRESULT
 * Requirements: 1.3
 */
HRESULT TSFBridge::startComposition(ITfContext* pContext) {
    if (!pContext) {
        return E_INVALIDARG;
    }
    
    // 如果已经有 composition，先结束它
    if (composition_) {
        endComposition();
    }
    
    // 获取 ITfContextComposition 接口
    ITfContextComposition* contextComposition = nullptr;
    HRESULT hr = pContext->QueryInterface(IID_ITfContextComposition, 
                                           reinterpret_cast<void**>(&contextComposition));
    if (FAILED(hr) || !contextComposition) {
        return hr;
    }
    
    // 获取当前插入点
    ITfInsertAtSelection* insertAtSelection = nullptr;
    hr = pContext->QueryInterface(IID_ITfInsertAtSelection, 
                                   reinterpret_cast<void**>(&insertAtSelection));
    if (FAILED(hr) || !insertAtSelection) {
        contextComposition->Release();
        return hr;
    }
    
    // 在当前选择位置插入空范围
    ITfRange* range = nullptr;
    hr = insertAtSelection->InsertTextAtSelection(
        windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE,
        TF_IAS_QUERYONLY,
        nullptr,
        0,
        &range
    );
    
    insertAtSelection->Release();
    
    if (FAILED(hr) || !range) {
        contextComposition->Release();
        return hr;
    }
    
    // 开始 composition
    hr = contextComposition->StartComposition(
        windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE,
        range,
        static_cast<ITfCompositionSink*>(this),
        &composition_
    );
    
    range->Release();
    contextComposition->Release();
    
    if (SUCCEEDED(hr) && composition_) {
        currentContext_ = pContext;
        
        // CUAS Workaround: 参考 weasel 的实现
        // 某些应用（CUAS - Cicero Unaware Application Services）不会提供正确的 GetTextExt 位置，
        // 除非 composition 中已经有字符。因此我们插入一个空格占位符。
        // 这个占位符会在实际 preedit 更新时被替换。
        ITfRange* compRange = nullptr;
        if (SUCCEEDED(composition_->GetRange(&compRange)) && compRange) {
            TfEditCookie ec = windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE;
            // 插入空格作为占位符
            compRange->SetText(ec, TF_ST_CORRECTION, L" ", 1);
            // 将光标移到开始位置
            compRange->Collapse(ec, TF_ANCHOR_START);
            compRange->Release();
        }
    }
    
    return hr;
}


/**
 * endComposition - 结束当前输入组合
 * Task 5.4: 实现 ITfCompositionSink
 * 
 * 结束当前的 TSF composition。
 * 
 * @return HRESULT
 * Requirements: 3.3
 */
HRESULT TSFBridge::endComposition() {
    if (!composition_) {
        return S_OK;
    }
    
    // 结束 composition
    HRESULT hr = composition_->EndComposition(
        windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE
    );
    
    // 释放 composition
    composition_->Release();
    composition_ = nullptr;
    
    return hr;
}

// 文本操作
/**
 * commitText - 提交文本到应用
 * Task 5.4: 实现文本提交
 * 
 * 通过 TSF 将文本提交到当前应用。
 * 
 * @param text 要提交的文本（UTF-16）
 * @return HRESULT
 * Requirements: 3.1, 3.3
 */
HRESULT TSFBridge::commitText(const std::wstring& text) {
    if (text.empty()) {
        return S_OK;
    }
    
    if (!currentContext_) {
        return E_FAIL;
    }
    
    // 如果有 composition，先结束它
    if (composition_) {
        // 获取 composition 的范围
        ITfRange* range = nullptr;
        HRESULT hr = composition_->GetRange(&range);
        
        if (SUCCEEDED(hr) && range) {
            // 设置文本到范围
            TfEditCookie ec = windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE;
            hr = range->SetText(ec, 0, text.c_str(), static_cast<LONG>(text.length()));
            range->Release();
        }
        
        // 结束 composition
        endComposition();
        
        return hr;
    }
    
    // 没有 composition，直接插入文本
    ITfInsertAtSelection* insertAtSelection = nullptr;
    HRESULT hr = currentContext_->QueryInterface(IID_ITfInsertAtSelection, 
                                                  reinterpret_cast<void**>(&insertAtSelection));
    if (FAILED(hr) || !insertAtSelection) {
        return hr;
    }
    
    TfEditCookie ec = windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE;
    ITfRange* range = nullptr;
    hr = insertAtSelection->InsertTextAtSelection(
        ec,
        0,
        text.c_str(),
        static_cast<LONG>(text.length()),
        &range
    );
    
    if (range) {
        range->Release();
    }
    insertAtSelection->Release();
    
    return hr;
}

/**
 * updatePreedit - 更新 preedit 文本
 * Task 5.4: 实现 preedit 更新
 * 
 * 更新 composition 中的 preedit 文本。
 * 
 * @param preedit preedit 文本（UTF-16）
 * @param caretPos 光标位置
 * @return HRESULT
 * Requirements: 4.1, 4.2
 */
HRESULT TSFBridge::updatePreedit(const std::wstring& preedit, int caretPos) {
    if (!currentContext_) {
        return E_FAIL;
    }
    
    // 如果没有 composition，创建一个
    if (!composition_) {
        HRESULT hr = startComposition(currentContext_);
        if (FAILED(hr)) {
            return hr;
        }
    }
    
    if (!composition_) {
        return E_FAIL;
    }
    
    // 获取 composition 的范围
    ITfRange* range = nullptr;
    HRESULT hr = composition_->GetRange(&range);
    if (FAILED(hr) || !range) {
        return hr;
    }
    
    // 设置 preedit 文本
    TfEditCookie ec = windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE;
    hr = range->SetText(ec, TF_ST_CORRECTION, preedit.c_str(), static_cast<LONG>(preedit.length()));
    
    range->Release();
    
    return hr;
}

/**
 * clearPreedit - 清除 preedit 文本
 * Task 5.4: 实现 preedit 清除
 * 
 * 清除当前的 preedit 文本并结束 composition。
 * 
 * @return HRESULT
 * Requirements: 4.3
 */
HRESULT TSFBridge::clearPreedit() {
    if (!composition_) {
        return S_OK;
    }
    
    // 获取 composition 的范围
    ITfRange* range = nullptr;
    HRESULT hr = composition_->GetRange(&range);
    
    if (SUCCEEDED(hr) && range) {
        // 清空文本
        TfEditCookie ec = windowsBridge_ ? windowsBridge_->getEditCookie() : TF_INVALID_COOKIE;
        range->SetText(ec, 0, L"", 0);
        range->Release();
    }
    
    // 结束 composition
    return endComposition();
}

// 私有方法
int TSFBridge::convertVirtualKeyToRime(WPARAM vk, LPARAM lParam) {
    UINT scanCode = (lParam >> 16) & 0xFF;
    bool extended = (lParam & (1 << 24)) != 0;
    return suyan::convertVirtualKeyToRime(vk, scanCode, extended);
}

int TSFBridge::convertModifiers() {
    return suyan::convertModifiersToRime();
}

HRESULT TSFBridge::initKeySink() {
    // Task 5.1: 初始化键盘事件接收器
    // 获取 ITfKeystrokeMgr 接口
    if (!threadMgr_) {
        return E_FAIL;
    }
    
    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    HRESULT hr = threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, 
                                             reinterpret_cast<void**>(&keystrokeMgr));
    if (FAILED(hr) || !keystrokeMgr) {
        return hr;
    }
    
    // 注册键盘事件接收器
    hr = keystrokeMgr->AdviseKeyEventSink(clientId_, 
                                           static_cast<ITfKeyEventSink*>(this), 
                                           TRUE);  // TRUE = 前台接收器
    
    keystrokeMgr->Release();
    
    if (SUCCEEDED(hr)) {
        keySinkCookie_ = 1;  // 标记已注册（实际 cookie 由 TSF 管理）
    }
    
    return hr;
}

HRESULT TSFBridge::uninitKeySink() {
    // Task 5.1: 清理键盘事件接收器
    if (!threadMgr_ || keySinkCookie_ == TF_INVALID_COOKIE) {
        return S_OK;
    }
    
    ITfKeystrokeMgr* keystrokeMgr = nullptr;
    HRESULT hr = threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, 
                                             reinterpret_cast<void**>(&keystrokeMgr));
    if (FAILED(hr) || !keystrokeMgr) {
        return hr;
    }
    
    // 注销键盘事件接收器
    hr = keystrokeMgr->UnadviseKeyEventSink(clientId_);
    
    keystrokeMgr->Release();
    keySinkCookie_ = TF_INVALID_COOKIE;
    
    return hr;
}

HRESULT TSFBridge::initThreadMgrSink() {
    // Task 5.1: 初始化线程管理器事件接收器
    ITfSource* source = nullptr;
    if (FAILED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        return E_FAIL;
    }

    HRESULT hr = source->AdviseSink(IID_ITfThreadMgrEventSink, 
                                    static_cast<ITfThreadMgrEventSink*>(this), 
                                    &threadMgrSinkCookie_);
    source->Release();
    
    if (FAILED(hr)) {
        threadMgrSinkCookie_ = TF_INVALID_COOKIE;
    }
    
    return hr;
}

HRESULT TSFBridge::uninitThreadMgrSink() {
    // Task 5.1: 清理线程管理器事件接收器
    if (threadMgrSinkCookie_ == TF_INVALID_COOKIE || !threadMgr_) {
        return S_OK;
    }

    ITfSource* source = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        source->UnadviseSink(threadMgrSinkCookie_);
        source->Release();
    }
    
    threadMgrSinkCookie_ = TF_INVALID_COOKIE;
    return S_OK;
}

BOOL TSFBridge::_InitTextEditSink(ITfDocumentMgr* pDocMgr) {
    // 清理旧的 Sink
    if (textEditSinkCookie_ != TF_INVALID_COOKIE || textLayoutSinkCookie_ != TF_INVALID_COOKIE) {
        if (textEditSinkContext_) {
            ITfSource* source = nullptr;
            if (SUCCEEDED(textEditSinkContext_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
                if (textEditSinkCookie_ != TF_INVALID_COOKIE) {
                    source->UnadviseSink(textEditSinkCookie_);
                    textEditSinkCookie_ = TF_INVALID_COOKIE;
                }
                if (textLayoutSinkCookie_ != TF_INVALID_COOKIE) {
                    source->UnadviseSink(textLayoutSinkCookie_);
                    textLayoutSinkCookie_ = TF_INVALID_COOKIE;
                }
                source->Release();
            }
            textEditSinkContext_->Release();
            textEditSinkContext_ = nullptr;
        }
    }

    if (!pDocMgr) {
        return TRUE;
    }

    // 获取顶层上下文
    if (FAILED(pDocMgr->GetTop(&textEditSinkContext_))) {
        return FALSE;
    }

    if (!textEditSinkContext_) {
        return TRUE;
    }

    // 注册新的 Sink
    BOOL fRet = FALSE;
    ITfSource* source = nullptr;
    if (SUCCEEDED(textEditSinkContext_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        // 注册 TextEditSink
        if (SUCCEEDED(source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &textEditSinkCookie_))) {
            fRet = TRUE;
        } else {
            textEditSinkCookie_ = TF_INVALID_COOKIE;
        }

        // 注册 TextLayoutSink (同时注册以确保能收到布局变更)
        if (SUCCEEDED(source->AdviseSink(IID_ITfTextLayoutSink, static_cast<ITfTextLayoutSink*>(this), &textLayoutSinkCookie_))) {
            fRet = TRUE;
        } else {
            textLayoutSinkCookie_ = TF_INVALID_COOKIE;
        }
        
        source->Release();
    }

    if (!fRet) {
        if (textEditSinkContext_) {
            textEditSinkContext_->Release();
            textEditSinkContext_ = nullptr;
        }
    }
    
    // 无论是新注册还是清空，都更新 currentContext_ 引用以便 bridge 其他部分使用正确的上下文
    // 注意：currentContext_ 主要用于 composition，可能需要更仔细的管理，但这里更新它是为了
    // 确保 TSFBridge 持有最新的焦点文档上下文。
    // 在 weasel 中，_pContextDocument 和 _pTextEditSinkContext 是分开管理的。
    // 这里我们先不强制覆盖 currentContext_，因为它通常由 StartComposition 时的上下文决定。

    return fRet;
}

void TSFBridge::updateCandidateWindowPosition() {
    DebugLog("updateCandidateWindowPosition called");
    
    if (!inputEngine_ || !candidateWindow_) {
        DebugLog("  -> Early return: inputEngine_=%p, candidateWindow_=%p", inputEngine_, candidateWindow_);
        return;
    }
    
    InputState state = inputEngine_->getState();
    DebugLog("  -> isComposing=%d, candidates=%zu", state.isComposing, state.candidates.size());
    
    if (!state.isComposing) {
        DebugLog("  -> Not composing, hiding window");
        QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
        if (QCoreApplication::instance()) {
            QCoreApplication::processEvents();
        }
        return;
    }
    
    // 策略改变：不再依赖异步 edit session，而是直接尝试获取位置
    // 因为在很多应用（如 Electron）中，edit session 可能永远不会完成
    
    RECT posRect = {0, 0, 0, 0};
    bool hasPosition = false;
    
    // 方法1: 使用 GetGUIThreadInfo（优先，因为它能获取前台窗口的正确光标位置）
    // 注意：GetCaretPos 在 TSF 上下文中返回的是输入法窗口的光标，不是目标应用的光标
    HWND hwnd = GetForegroundWindow();
    DebugLog("  -> GetForegroundWindow: %p", hwnd);
    if (hwnd) {
        DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
        DebugLog("  -> Target thread ID: %lu", threadId);
        if (threadId != 0) {
            GUITHREADINFO gti;
            gti.cbSize = sizeof(GUITHREADINFO);
            if (GetGUIThreadInfo(threadId, &gti)) {
                DebugLog("  -> GUITHREADINFO: hwndFocus=%p, hwndCaret=%p, rcCaret=(%d,%d,%d,%d)",
                         gti.hwndFocus, gti.hwndCaret,
                         gti.rcCaret.left, gti.rcCaret.top, gti.rcCaret.right, gti.rcCaret.bottom);
                         
                if (gti.rcCaret.left != 0 || gti.rcCaret.top != 0 ||
                    gti.rcCaret.right != 0 || gti.rcCaret.bottom != 0) {
                    // 使用 hwndCaret 或 hwndFocus 来转换坐标
                    HWND caretWnd = gti.hwndCaret ? gti.hwndCaret : 
                                   (gti.hwndFocus ? gti.hwndFocus : hwnd);
                    
                    // 调试：获取窗口位置
                    RECT wndRect;
                    GetWindowRect(caretWnd, &wndRect);
                    DebugLog("  -> caretWnd=%p, WindowRect=(%d,%d,%d,%d)", 
                             caretWnd, wndRect.left, wndRect.top, wndRect.right, wndRect.bottom);
                    
                    POINT topLeft = { gti.rcCaret.left, gti.rcCaret.top };
                    POINT bottomRight = { gti.rcCaret.right, gti.rcCaret.bottom };
                    DebugLog("  -> Before ClientToScreen: topLeft=(%d,%d)", topLeft.x, topLeft.y);
                    ClientToScreen(caretWnd, &topLeft);
                    ClientToScreen(caretWnd, &bottomRight);
                    DebugLog("  -> After ClientToScreen: topLeft=(%d,%d)", topLeft.x, topLeft.y);
                    
                    posRect.left = topLeft.x;
                    posRect.top = topLeft.y;
                    posRect.right = bottomRight.x;
                    posRect.bottom = bottomRight.y;
                    if (posRect.bottom <= posRect.top) {
                        posRect.bottom = posRect.top + 20;
                    }
                    hasPosition = true;
                    DebugLog("  -> Position from GUITHREADINFO: (%d, %d) to (%d, %d)",
                             posRect.left, posRect.top, posRect.right, posRect.bottom);
                }

            } else {
                DebugLog("  -> GetGUIThreadInfo failed");
            }
        }
    }
    
    // 方法2: 如果 GetGUIThreadInfo 失败，尝试使用 GetCaretPos（可能不准确）
    if (!hasPosition) {
        POINT caretPt;
        if (::GetCaretPos(&caretPt)) {
            HWND focusWnd = GetFocus();
            DebugLog("  -> GetCaretPos fallback: (%d, %d), focusWnd=%p", caretPt.x, caretPt.y, focusWnd);
            if (focusWnd) {
                ::ClientToScreen(focusWnd, &caretPt);
                posRect.left = caretPt.x;
                posRect.top = caretPt.y;
                posRect.right = caretPt.x + 2;
                posRect.bottom = caretPt.y + 20;
                hasPosition = true;
                DebugLog("  -> Position from GetCaretPos: (%d, %d)", posRect.left, posRect.top);
            }
        } else {
            DebugLog("  -> GetCaretPos failed");
        }
    }

    
    // 方法3: 使用鼠标位置作为最后回退
    if (!hasPosition) {
        POINT pt;
        if (GetCursorPos(&pt)) {
            posRect.left = pt.x;
            posRect.top = pt.y + 20;
            posRect.right = pt.x + 2;
            posRect.bottom = pt.y + 40;
            hasPosition = true;
            DebugLog("  -> Position from mouse: (%d, %d)", posRect.left, posRect.top);
        }
    }
    
    // 显示候选窗口
    if (hasPosition) {
        DebugLog("  -> Calling setCompositionPosition with (%d, %d, %d, %d)", 
                 posRect.left, posRect.top, posRect.right, posRect.bottom);
        setCompositionPosition(posRect);
    } else {
        DebugLog("  -> No position found, not showing window");
    }
    
    // 同时也尝试通过 edit session 获取更精确的位置（如果应用支持的话）
    if (currentContext_) {
        auto* editSession = new GetTextExtEditSession(currentContext_, composition_, this);
        HRESULT hrSession = S_OK;
        currentContext_->RequestEditSession(
            clientId_,
            editSession,
            TF_ES_ASYNCDONTCARE | TF_ES_READ,
            &hrSession
        );
        editSession->Release();
    }
}




void TSFBridge::setCompositionPosition(const RECT& rc) {
    if (!candidateWindow_ || !inputEngine_) {
        return;
    }
    
    InputState state = inputEngine_->getState();
    if (!state.isComposing) {
        return;
    }
    
    RECT validRect = rc;
    
    // 如果位置无效（全为0或负数），尝试使用回退方法
    if (validRect.left <= 0 && validRect.top <= 0 && 
        validRect.right <= 0 && validRect.bottom <= 0) {
        
        // 回退方法1: 使用 GetGUIThreadInfo
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
            if (threadId != 0) {
                GUITHREADINFO gti;
                gti.cbSize = sizeof(GUITHREADINFO);
                if (GetGUIThreadInfo(threadId, &gti)) {
                    if (gti.rcCaret.left != 0 || gti.rcCaret.top != 0 ||
                        gti.rcCaret.right != 0 || gti.rcCaret.bottom != 0) {
                        HWND caretWnd = gti.hwndCaret ? gti.hwndCaret : 
                                       (gti.hwndFocus ? gti.hwndFocus : hwnd);
                        POINT topLeft = { gti.rcCaret.left, gti.rcCaret.bottom };
                        ClientToScreen(caretWnd, &topLeft);
                        validRect.left = topLeft.x;
                        validRect.top = topLeft.y;
                        validRect.right = topLeft.x + 2;
                        validRect.bottom = topLeft.y + (gti.rcCaret.bottom - gti.rcCaret.top);
                        if (validRect.bottom <= validRect.top) {
                            validRect.bottom = validRect.top + 20;
                        }
                    }
                }
            }
        }
        
        // 回退方法2: 使用 GetCaretPos
        if (validRect.left <= 0 && validRect.top <= 0) {
            POINT caretPos;
            if (GetCaretPos(&caretPos)) {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    ClientToScreen(hwndFocus, &caretPos);
                    validRect.left = caretPos.x;
                    validRect.top = caretPos.y;
                    validRect.right = caretPos.x + 2;
                    validRect.bottom = caretPos.y + 20;
                }
            }
        }
        
        // 回退方法3: 使用鼠标位置
        if (validRect.left <= 0 && validRect.top <= 0) {
            POINT pt;
            if (GetCursorPos(&pt)) {
                validRect.left = pt.x;
                validRect.top = pt.y + 20;  // 在鼠标下方
                validRect.right = pt.x + 2;
                validRect.bottom = pt.y + 40;
            }
        }
    }
    
    // 如果仍然无效，不显示窗口
    if (validRect.left <= 0 && validRect.top <= 0) {
        DebugLog("setCompositionPosition: invalid position, not showing");
        return;
    }
    
    // Task 6: DPI 缩放修正 (Native Direct Strategy)
    // TSF/Windows API 返回的是物理坐标 (Physical Coordinates)
    // 我们直接使用 SetWindowPos (通过 showAtNative) 设置物理坐标
    // 这样彻底绕过了 Qt 的 DPI 映射和屏幕枚举问题
    // 完美解决 多显示器 + Chrome/Electron 沙盒 + 混合 DPI 问题
    
    // 构造物理坐标矩形 (Physical Rect)
    QRect physicalRect(validRect.left, validRect.top, 
                       validRect.right - validRect.left, 
                       validRect.bottom - validRect.top);
    
    DebugLog("setCompositionPosition: Native Force Rect (%d,%d %dx%d)", 
             physicalRect.x(), physicalRect.y(), physicalRect.width(), physicalRect.height());
    
    QMetaObject::invokeMethod(candidateWindow_, [this, physicalRect]() {
        if (candidateWindow_) {
            DebugLog("  -> invokeMethod calling showAtNative");
            candidateWindow_->showAtNative(physicalRect);
        }
    }, Qt::QueuedConnection);
    
    // 手动处理 Qt 事件队列（DLL 中没有事件循环）
    if (QCoreApplication::instance()) {
        QCoreApplication::processEvents();
    }
}



/**
 * handleShiftKeyRelease - 处理 Shift 键释放切换模式
 * Task 5.2: 实现 ITfKeyEventSink
 * 
 * 当 Shift 键单独按下并释放时，切换中英文模式。
 * 如果正在输入，先提交原始拼音再切换。
 */
void TSFBridge::handleShiftKeyRelease() {
    if (!inputEngine_) {
        return;
    }
    
    // 如果正在输入中文，提交当前的原始拼音字母
    if (inputEngine_->isComposing()) {
        InputState state = inputEngine_->getState();
        if (!state.rawInput.empty() && windowsBridge_) {
            // 提交原始拼音
            windowsBridge_->commitText(state.rawInput);
        }
        // 重置输入状态
        inputEngine_->reset();
        // 隐藏候选词窗口
        if (candidateWindow_) {
            QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
            if (QCoreApplication::instance()) {
                QCoreApplication::processEvents();
            }
        }
    }
    
    // 切换模式
    inputEngine_->toggleMode();
}

// ============================================
// ITfThreadMgrEventSink 实现
// ============================================

STDMETHODIMP TSFBridge::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) {
    return S_OK;
}

STDMETHODIMP TSFBridge::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) {
    return S_OK;
}

STDMETHODIMP TSFBridge::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) {
    // 关键：当文档焦点变化时，切换 TextEditSink 监听目标
    _InitTextEditSink(pDocMgrFocus);
    
    // 如果失去了焦点，隐藏候选窗口
    if (!pDocMgrFocus && candidateWindow_) {
        QMetaObject::invokeMethod(candidateWindow_, "hideWindow", Qt::QueuedConnection);
        if (QCoreApplication::instance()) {
            QCoreApplication::processEvents();
        }
    }
    
    return S_OK;
}

STDMETHODIMP TSFBridge::OnPushContext(ITfContext* pContext) {
    return S_OK;
}

STDMETHODIMP TSFBridge::OnPopContext(ITfContext* pContext) {
    return S_OK;
}

// ============================================
// ITfTextEditSink 实现
// ============================================

// 辅助函数：判断 rangeCover 是否覆盖 rangeTest (用于检测光标是否移出 composition)
static BOOL IsRangeCovered(TfEditCookie ec, ITfRange* pRangeTest, ITfRange* pRangeCover) {
    LONG lResult;
    if (pRangeCover->CompareStart(ec, pRangeTest, TF_ANCHOR_START, &lResult) != S_OK || lResult > 0)
        return FALSE;
    if (pRangeCover->CompareEnd(ec, pRangeTest, TF_ANCHOR_END, &lResult) != S_OK || lResult < 0)
        return FALSE;
    return TRUE;
}

STDMETHODIMP TSFBridge::OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord) {
    // 检查选择（光标）变化
    BOOL fSelectionChanged;
    if (SUCCEEDED(pEditRecord->GetSelectionStatus(&fSelectionChanged)) && fSelectionChanged) {
        if (isComposing() && composition_) {
            // 获取当前选择
            TF_SELECTION tfSelection;
            ULONG cFetched;
            if (SUCCEEDED(pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &tfSelection, &cFetched)) && cFetched == 1) {
                // 获取 composition 范围
                ITfRange* pRangeComposition = nullptr;
                if (SUCCEEDED(composition_->GetRange(&pRangeComposition)) && pRangeComposition) {
                    // 如果光标移出了 composition 范围，结束 composition
                    if (!IsRangeCovered(ecReadOnly, tfSelection.range, pRangeComposition)) {
                        DebugLog("Cursor moved out of composition range, ending composition");
                        endComposition();
                    }
                    pRangeComposition->Release();
                }
                if (tfSelection.range) {
                    tfSelection.range->Release();
                }
            }
        }
    }
    return S_OK;
}

// ============================================
// TSFBridgeFactory 实现
// ============================================

STDMETHODIMP TSFBridgeFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TSFBridgeFactory::AddRef() {
    return 1; // 静态工厂，不需要引用计数
}

STDMETHODIMP_(ULONG) TSFBridgeFactory::Release() {
    return 1; // 静态工厂，不需要引用计数
}

STDMETHODIMP TSFBridgeFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, 
                                               void** ppvObj) {
    if (!ppvObj) {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (pUnkOuter) {
        return CLASS_E_NOAGGREGATION;
    }

    TSFBridge* bridge = new (std::nothrow) TSFBridge();
    if (!bridge) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = bridge->QueryInterface(riid, ppvObj);
    bridge->Release();

    return hr;
}

STDMETHODIMP TSFBridgeFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_serverLockCount);
    } else {
        InterlockedDecrement(&g_serverLockCount);
    }
    return S_OK;
}

// ============================================
// DLL 导出函数
// ============================================

static TSFBridgeFactory g_factory;

extern "C" {

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) {
        return E_INVALIDARG;
    }

    *ppv = nullptr;

    if (!IsEqualCLSID(rclsid, CLSID_SuYanTextService)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() {
    return (g_dllRefCount == 0 && g_serverLockCount == 0) ? S_OK : S_FALSE;
}

/**
 * 注册 COM 组件到注册表
 * 
 * 创建以下注册表项：
 * HKCR\CLSID\{CLSID}\
 *   (Default) = "SuYan Input Method"
 *   InprocServer32\
 *     (Default) = <DLL 路径>
 *     ThreadingModel = "Apartment"
 */
static HRESULT RegisterCOMServer() {
    wchar_t clsidStr[64];
    if (!GuidToString(CLSID_SuYanTextService, clsidStr, 64)) {
        return E_FAIL;
    }

    // 获取 DLL 路径
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, dllPath, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT hr;

    // 创建 CLSID 键
    wchar_t keyPath[256];
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s", clsidStr);
    hr = CreateRegKeyAndSetValue(HKEY_CLASSES_ROOT, keyPath, nullptr, L"SuYan Input Method");
    if (FAILED(hr)) return hr;

    // 创建 InprocServer32 键
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s\\InprocServer32", clsidStr);
    hr = CreateRegKeyAndSetValue(HKEY_CLASSES_ROOT, keyPath, nullptr, dllPath);
    if (FAILED(hr)) return hr;

    // 设置 ThreadingModel
    hr = CreateRegKeyAndSetValue(HKEY_CLASSES_ROOT, keyPath, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    return S_OK;
}

/**
 * 注销 COM 组件
 */
static HRESULT UnregisterCOMServer() {
    wchar_t clsidStr[64];
    if (!GuidToString(CLSID_SuYanTextService, clsidStr, 64)) {
        return E_FAIL;
    }

    wchar_t keyPath[256];
    StringCchPrintfW(keyPath, 256, L"CLSID\\%s", clsidStr);
    
    return DeleteRegKey(HKEY_CLASSES_ROOT, keyPath);
}

/**
 * 注册 TSF 输入法配置
 * 
 * 使用 ITfInputProcessorProfiles 接口注册输入法
 */
static HRESULT RegisterTSFProfile() {
    HRESULT hr;
    ITfInputProcessorProfiles* pProfiles = nullptr;

    // 创建 ITfInputProcessorProfiles 实例
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&pProfiles));
    if (FAILED(hr)) {
        return hr;
    }

    // 注册输入法
    hr = pProfiles->Register(CLSID_SuYanTextService);
    if (FAILED(hr)) {
        pProfiles->Release();
        return hr;
    }

    // 获取 DLL 路径（用于图标）
    wchar_t dllPath[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, dllPath, MAX_PATH) == 0) {
        pProfiles->Release();
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // 添加语言配置
    // LANGID: 0x0804 = 简体中文
    hr = pProfiles->AddLanguageProfile(
        CLSID_SuYanTextService,           // CLSID
        SUYAN_LANGID,                     // 语言 ID (简体中文)
        GUID_SuYanProfile,                // 配置 GUID
        L"素言输入法",                     // 显示名称
        static_cast<ULONG>(wcslen(L"素言输入法")),  // 名称长度
        dllPath,                          // 图标文件
        static_cast<ULONG>(wcslen(dllPath)),        // 图标文件路径长度
        0                                 // 图标索引
    );

    pProfiles->Release();
    return hr;
}

/**
 * 注销 TSF 输入法配置
 */
static HRESULT UnregisterTSFProfile() {
    HRESULT hr;
    ITfInputProcessorProfiles* pProfiles = nullptr;

    // 创建 ITfInputProcessorProfiles 实例
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&pProfiles));
    if (FAILED(hr)) {
        return hr;
    }

    // 注销输入法
    hr = pProfiles->Unregister(CLSID_SuYanTextService);

    pProfiles->Release();
    return hr;
}

/**
 * 注册 TSF 分类（Category）
 * 
 * 将输入法注册到以下分类：
 * - GUID_TFCAT_TIP_KEYBOARD: 键盘输入法
 * - GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER: 显示属性提供器
 */
static HRESULT RegisterTSFCategories() {
    HRESULT hr;
    ITfCategoryMgr* pCategoryMgr = nullptr;

    // 创建 ITfCategoryMgr 实例
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfCategoryMgr, reinterpret_cast<void**>(&pCategoryMgr));
    if (FAILED(hr)) {
        return hr;
    }

    // 注册为键盘输入法
    hr = pCategoryMgr->RegisterCategory(
        CLSID_SuYanTextService,           // CLSID
        GUID_TFCAT_TIP_KEYBOARD,          // 分类 GUID
        CLSID_SuYanTextService            // 项目 GUID
    );
    if (FAILED(hr)) {
        pCategoryMgr->Release();
        return hr;
    }

    // 注册为显示属性提供器（可选，用于 preedit 样式）
    hr = pCategoryMgr->RegisterCategory(
        CLSID_SuYanTextService,
        GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
        CLSID_SuYanTextService
    );
    // 这个注册失败不是致命错误，继续执行

    pCategoryMgr->Release();
    return S_OK;
}

/**
 * 注销 TSF 分类
 */
static HRESULT UnregisterTSFCategories() {
    HRESULT hr;
    ITfCategoryMgr* pCategoryMgr = nullptr;

    // 创建 ITfCategoryMgr 实例
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITfCategoryMgr, reinterpret_cast<void**>(&pCategoryMgr));
    if (FAILED(hr)) {
        return hr;
    }

    // 注销键盘输入法分类
    pCategoryMgr->UnregisterCategory(
        CLSID_SuYanTextService,
        GUID_TFCAT_TIP_KEYBOARD,
        CLSID_SuYanTextService
    );

    // 注销显示属性提供器分类
    pCategoryMgr->UnregisterCategory(
        CLSID_SuYanTextService,
        GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
        CLSID_SuYanTextService
    );

    pCategoryMgr->Release();
    return S_OK;
}

/**
 * DllRegisterServer - 注册 DLL
 * 
 * 执行以下注册步骤：
 * 1. 注册 COM 服务器
 * 2. 注册 TSF 输入法配置
 * 3. 注册 TSF 分类
 */
STDAPI DllRegisterServer() {
    HRESULT hr;

    // 初始化 COM（如果尚未初始化）
    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hr);

    // 1. 注册 COM 服务器
    hr = RegisterCOMServer();
    if (FAILED(hr)) {
        if (comInitialized) CoUninitialize();
        return hr;
    }

    // 2. 注册 TSF 输入法配置
    hr = RegisterTSFProfile();
    if (FAILED(hr)) {
        // 回滚 COM 注册
        UnregisterCOMServer();
        if (comInitialized) CoUninitialize();
        return hr;
    }

    // 3. 注册 TSF 分类
    hr = RegisterTSFCategories();
    if (FAILED(hr)) {
        // 回滚之前的注册
        UnregisterTSFProfile();
        UnregisterCOMServer();
        if (comInitialized) CoUninitialize();
        return hr;
    }

    if (comInitialized) CoUninitialize();
    return S_OK;
}

/**
 * DllUnregisterServer - 注销 DLL
 * 
 * 执行以下注销步骤（顺序与注册相反）：
 * 1. 注销 TSF 分类
 * 2. 注销 TSF 输入法配置
 * 3. 注销 COM 服务器
 */
STDAPI DllUnregisterServer() {
    // 初始化 COM（如果尚未初始化）
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hrInit);

    // 1. 注销 TSF 分类
    UnregisterTSFCategories();

    // 2. 注销 TSF 输入法配置
    UnregisterTSFProfile();

    // 3. 注销 COM 服务器
    HRESULT hr = UnregisterCOMServer();

    if (comInitialized) CoUninitialize();
    return hr;
}

} // extern "C"

} // namespace suyan

#endif // _WIN32
