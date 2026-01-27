/**
 * WindowsBridge - Windows 平台桥接实现
 * Task 3: WindowsBridge 实现
 * 
 * 实现 IPlatformBridge 接口
 * Requirements: 3.2, 3.4, 4.4, 4.5, 5.1, 10.1
 */

#ifndef SUYAN_WINDOWS_WINDOWS_BRIDGE_H
#define SUYAN_WINDOWS_WINDOWS_BRIDGE_H

#ifdef _WIN32

// Qt 头文件必须在 Windows 头文件之前包含
// 因为 Windows 头文件定义了 Bool 宏，与 Qt 的 qmetatype.h 冲突
#include <QtCore/qglobal.h>

#include "platform_bridge.h"
#include "tsf_types.h"
#include <windows.h>
#include <msctf.h>
#include <string>

namespace suyan {

// 前向声明
class TSFBridge;

/**
 * WindowsBridge - Windows 平台桥接实现
 * 
 * 实现 IPlatformBridge 接口，提供：
 * - 文本提交到当前应用
 * - 获取光标位置
 * - 更新/清除 preedit
 * - 获取当前应用信息
 */
class WindowsBridge : public IPlatformBridge {
public:
    WindowsBridge();
    ~WindowsBridge() override;

    // ========== IPlatformBridge 接口实现 ==========

    /**
     * 提交文字到当前应用
     * 通过 TSF ITfContext 提交，或回退到 SendInput
     * Requirements: 3.1, 3.3, 3.5
     */
    void commitText(const std::string& text) override;

    /**
     * 获取当前光标位置（屏幕坐标）
     * 通过 ITfContextView::GetTextExt 获取
     * Requirements: 5.2, 5.3, 5.4
     */
    CursorPosition getCursorPosition() override;

    /**
     * 更新 preedit（输入中的拼音显示在应用内）
     * 通过 TSF composition 更新
     * Requirements: 4.1, 4.2, 4.3
     */
    void updatePreedit(const std::string& preedit, int caretPos) override;

    /**
     * 清除 preedit
     * Requirements: 4.3
     */
    void clearPreedit() override;

    /**
     * 获取当前应用的进程名
     * 返回如 "notepad.exe"
     * Requirements: 10.2, 10.3
     */
    std::string getCurrentAppId() override;

    // ========== Windows 特定方法 ==========

    /**
     * 设置 TSFBridge 引用
     */
    void setTSFBridge(TSFBridge* bridge) { tsfBridge_ = bridge; }

    /**
     * 获取 TSFBridge 引用
     */
    TSFBridge* getTSFBridge() const { return tsfBridge_; }

    /**
     * 设置当前 TSF 上下文
     */
    void setContext(ITfContext* context) { currentContext_ = context; }

    /**
     * 获取当前 TSF 上下文
     */
    ITfContext* getContext() const { return currentContext_; }

    /**
     * 设置编辑 Cookie（用于 TSF 编辑会话）
     */
    void setEditCookie(TfEditCookie cookie) { editCookie_ = cookie; }

    /**
     * 获取编辑 Cookie
     */
    TfEditCookie getEditCookie() const { return editCookie_; }

    // ========== 编码转换（公开用于测试） ==========

    /**
     * UTF-8 到 UTF-16 转换
     * Requirements: 3.2
     */
    static std::wstring utf8ToWide(const std::string& utf8);
    
    /**
     * UTF-16 到 UTF-8 转换
     * Requirements: 3.2
     */
    static std::string wideToUtf8(const std::wstring& wide);

private:
    // 通过 TSF 提交文本
    bool commitTextViaTSF(const std::wstring& text);

    // 通过 SendInput 回退提交
    void commitTextViaSendInput(const std::wstring& text);

    // 获取前台窗口的进程名
    std::string getForegroundProcessName();

    // 获取光标矩形（通过 GetGUIThreadInfo，最可靠）
    bool getCursorRectFromGUIThread(CaretRect& rect);

    // 获取光标矩形（通过 TSF）
    bool getCursorRectFromTSF(CaretRect& rect);

    // 获取光标矩形（通过 GetCaretPos 回退）
    bool getCursorRectFromCaret(CaretRect& rect);
    
    // 获取光标位置（通过鼠标位置作为最后回退）
    bool getCursorRectFromMousePos(CaretRect& rect);

    TSFBridge* tsfBridge_ = nullptr;
    ITfContext* currentContext_ = nullptr;
    TfEditCookie editCookie_ = TF_INVALID_COOKIE;
    
    // 保存最后有效的 composition 位置（用于回退）
    RECT lastCompositionRect_ = {0, 0, 0, 0};
    
public:
    // 设置最后有效的 composition 位置
    void setLastCompositionRect(const RECT& rect) { lastCompositionRect_ = rect; }
    const RECT& getLastCompositionRect() const { return lastCompositionRect_; }
};

} // namespace suyan

#endif // _WIN32

#endif // SUYAN_WINDOWS_WINDOWS_BRIDGE_H
