/**
 * TSFBridge - TSF 文本服务实现
 * Task 1.1: Windows 平台目录结构
 * 
 * 实现 TSF 所需的 COM 接口
 */

#ifndef SUYAN_WINDOWS_TSF_BRIDGE_H
#define SUYAN_WINDOWS_TSF_BRIDGE_H

#ifdef _WIN32

// Qt 头文件必须在 Windows 头文件之前包含
// 因为 Windows 头文件定义了 Bool 宏，与 Qt 的 qmetatype.h 冲突
#include <QtCore/qglobal.h>

#include <windows.h>
#include <msctf.h>
#include <atomic>
#include <string>

namespace suyan {

// 前向声明
class InputEngine;
class CandidateWindow;
class WindowsBridge;
class TSFBridge;

// ============================================
// CLSID 和 GUID 声明
// ============================================

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
// 素言输入法 CLSID
extern const CLSID CLSID_SuYanTextService;

// {B2C3D4E5-F6A7-8901-BCDE-F12345678901}
// 素言输入法语言配置 GUID
extern const GUID GUID_SuYanProfile;

// 语言 ID: 简体中文
constexpr LANGID SUYAN_LANGID = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

// ============================================
// GetTextExtEditSession - 用于获取光标位置的编辑会话
// ============================================

class GetTextExtEditSession : public ITfEditSession {
public:
    GetTextExtEditSession(ITfContext* pContext, ITfComposition* pComposition, TSFBridge* pBridge);
    virtual ~GetTextExtEditSession();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

    // 获取结果
    RECT getTextRect() const { return textRect_; }
    bool isValid() const { return isValid_; }

private:
    std::atomic<ULONG> refCount_{1};
    ITfContext* context_;
    ITfComposition* composition_;
    TSFBridge* bridge_;
    RECT textRect_;
    bool isValid_;
};

// ============================================
// TSFBridge 类
// ============================================

/**
 * TSFBridge - TSF 文本服务实现
 * 
 * 实现 TSF 所需的 COM 接口：
 * - IUnknown: COM 基础接口
 * - ITfTextInputProcessor: 文本输入处理器
 * - ITfKeyEventSink: 键盘事件接收器
 * - ITfCompositionSink: 输入组合接收器
 * - ITfDisplayAttributeProvider: 显示属性提供器
 * - ITfTextLayoutSink: 文本布局变化通知
 */
class TSFBridge : public ITfTextInputProcessor,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfDisplayAttributeProvider,
                  public ITfTextLayoutSink,
                  public ITfThreadMgrEventSink,
                  public ITfTextEditSink {
public:
    TSFBridge();
    virtual ~TSFBridge();

    // ========== IUnknown ==========
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ========== ITfTextInputProcessor ==========
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ========== ITfKeyEventSink ==========
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, 
                               LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, 
                             LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, 
                           LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, 
                         LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, 
                                BOOL* pfEaten) override;

    // ========== ITfCompositionSink ==========
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, 
                                         ITfComposition* pComposition) override;

    // ========== ITfDisplayAttributeProvider ==========
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, 
                                         ITfDisplayAttributeInfo** ppInfo) override;

    // ========== ITfTextLayoutSink ==========
    STDMETHODIMP OnLayoutChange(ITfContext* pContext, TfLayoutCode lcode, 
                                ITfContextView* pView) override;

    // ========== ITfThreadMgrEventSink ==========
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pContext) override;
    STDMETHODIMP OnPopContext(ITfContext* pContext) override;

    // ========== ITfTextEditSink ==========
    STDMETHODIMP OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly, 
                           ITfEditRecord* pEditRecord) override;

    // ========== 组件访问 ==========
    void setInputEngine(InputEngine* engine) { inputEngine_ = engine; }
    void setCandidateWindow(CandidateWindow* window) { candidateWindow_ = window; }
    void setWindowsBridge(WindowsBridge* bridge) { windowsBridge_ = bridge; }

    InputEngine* getInputEngine() const { return inputEngine_; }
    CandidateWindow* getCandidateWindow() const { return candidateWindow_; }
    WindowsBridge* getWindowsBridge() const { return windowsBridge_; }

    // ========== 状态访问 ==========
    ITfThreadMgr* getThreadMgr() const { return threadMgr_; }
    TfClientId getClientId() const { return clientId_; }
    ITfContext* getCurrentContext() const { return currentContext_; }

    // ========== 输入组合管理 ==========
    HRESULT startComposition(ITfContext* pContext);
    HRESULT endComposition();
    bool isComposing() const { return composition_ != nullptr; }

    // ========== 文本操作 ==========
    HRESULT commitText(const std::wstring& text);
    HRESULT updatePreedit(const std::wstring& preedit, int caretPos);
    HRESULT clearPreedit();
    
    // ========== 候选窗口位置 ==========
    void setCompositionPosition(const RECT& rc);

    // ========== 激活状态 ==========
    bool isActivated() const { return activated_; }

private:
    // 键码转换
    int convertVirtualKeyToRime(WPARAM vk, LPARAM lParam);
    int convertModifiers();

    // 初始化和清理
    HRESULT initKeySink();
    HRESULT uninitKeySink();
    HRESULT initThreadMgrSink();
    HRESULT uninitThreadMgrSink();
    
    // 文本编辑 Sink 初始化 (替代原有的 initTextLayoutSink)
    BOOL _InitTextEditSink(ITfDocumentMgr* pDocMgr);
    
    // 更新候选词窗口位置
    void updateCandidateWindowPosition();

    // 处理 Shift 键切换模式
    void handleShiftKeyRelease();

    // COM 引用计数
    std::atomic<ULONG> refCount_{1};

    // TSF 对象
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    ITfContext* currentContext_ = nullptr;
    ITfComposition* composition_ = nullptr;
    ITfCompositionSink* compositionSink_ = nullptr;
    DWORD threadMgrSinkCookie_ = TF_INVALID_COOKIE;
    DWORD textEditSinkCookie_ = TF_INVALID_COOKIE;
    DWORD keySinkCookie_ = TF_INVALID_COOKIE;
    DWORD textLayoutSinkCookie_ = TF_INVALID_COOKIE;
    
    // 当前监听的文档上下文 (用于 TextEditSink)
    ITfContext* textEditSinkContext_ = nullptr;

    // 激活状态
    bool activated_ = false;

    // Shift 键状态跟踪（用于中英文切换）
    bool shiftKeyPressed_ = false;
    bool otherKeyPressedWithShift_ = false;
    DWORD shiftPressTime_ = 0;

    // 组件引用
    InputEngine* inputEngine_ = nullptr;
    CandidateWindow* candidateWindow_ = nullptr;
    WindowsBridge* windowsBridge_ = nullptr;
};

// ============================================
// COM 类工厂
// ============================================

class TSFBridgeFactory : public IClassFactory {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, 
                                void** ppvObj) override;
    STDMETHODIMP LockServer(BOOL fLock) override;
};

// ============================================
// DLL 导出函数
// ============================================

extern "C" {
    STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv);
    STDAPI DllCanUnloadNow();
    STDAPI DllRegisterServer();
    STDAPI DllUnregisterServer();
}

// ============================================
// DLL 模块管理
// ============================================

/**
 * 获取 DLL 模块句柄
 */
HMODULE GetModuleHandle();

/**
 * 设置 DLL 模块句柄（在 DllMain 中调用）
 */
void SetModuleHandle(HMODULE hModule);

/**
 * 增加 DLL 引用计数
 */
void DllAddRef();

/**
 * 减少 DLL 引用计数
 */
void DllRelease();

// ============================================
// 组件初始化和访问（Task 8）
// ============================================

/**
 * 初始化所有组件
 * 在 TSFBridge::Activate 首次调用时执行
 * Requirements: 12.1, 12.2, 13.1, 13.2, 13.3
 */
bool InitializeComponents();

/**
 * 清理所有组件
 * Requirements: 12.3
 */
void CleanupComponents();

/**
 * 显示错误对话框
 * Requirements: 13.4
 */
void ShowErrorDialog(const wchar_t* title, const wchar_t* message);

/**
 * 获取 InputEngine 实例
 */
InputEngine* GetInputEngine();

/**
 * 获取 CandidateWindow 实例
 */
CandidateWindow* GetCandidateWindow();

/**
 * 获取 WindowsBridge 实例
 */
WindowsBridge* GetWindowsBridge();

/**
 * 检查是否已初始化
 */
bool IsInitialized();

} // namespace suyan

#endif // _WIN32

#endif // SUYAN_WINDOWS_TSF_BRIDGE_H
