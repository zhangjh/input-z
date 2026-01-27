/**
 * WindowsBridge 实现
 * Task 3: WindowsBridge 实现
 * 
 * 实现 IPlatformBridge 接口的 Windows 平台版本
 * Requirements: 3.1-3.5, 4.1-4.5, 5.1-5.4, 10.1-10.3
 */

#ifdef _WIN32

#include "windows_bridge.h"
#include "tsf_bridge.h"
#include <psapi.h>
#include <ctffunc.h>

// 链接 psapi 库
#pragma comment(lib, "psapi.lib")

namespace suyan {

// ============================================
// 构造和析构
// ============================================

WindowsBridge::WindowsBridge() = default;

WindowsBridge::~WindowsBridge() = default;

// ============================================
// 编码转换实现
// Requirements: 3.2
// ============================================

std::wstring WindowsBridge::utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    
    // 计算所需的宽字符长度
    int wideLen = MultiByteToWideChar(
        CP_UTF8, 
        0, 
        utf8.c_str(), 
        static_cast<int>(utf8.length()), 
        nullptr, 
        0
    );
    
    if (wideLen == 0) {
        return std::wstring();
    }
    
    // 分配缓冲区并转换
    std::wstring wide(wideLen, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 
        0, 
        utf8.c_str(), 
        static_cast<int>(utf8.length()), 
        &wide[0], 
        wideLen
    );
    
    return wide;
}

std::string WindowsBridge::wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return std::string();
    }
    
    // 计算所需的 UTF-8 字节长度
    int utf8Len = WideCharToMultiByte(
        CP_UTF8, 
        0, 
        wide.c_str(), 
        static_cast<int>(wide.length()), 
        nullptr, 
        0, 
        nullptr, 
        nullptr
    );
    
    if (utf8Len == 0) {
        return std::string();
    }
    
    // 分配缓冲区并转换
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(
        CP_UTF8, 
        0, 
        wide.c_str(), 
        static_cast<int>(wide.length()), 
        &utf8[0], 
        utf8Len, 
        nullptr, 
        nullptr
    );
    
    return utf8;
}

// ============================================
// commitText 实现
// Requirements: 3.1, 3.3, 3.5
// ============================================

void WindowsBridge::commitText(const std::string& text) {
    if (text.empty()) {
        return;
    }
    
    std::wstring wideText = utf8ToWide(text);
    if (wideText.empty()) {
        return;
    }
    
    // 首先尝试通过 TSF 提交
    if (commitTextViaTSF(wideText)) {
        return;
    }
    
    // TSF 提交失败，回退到 SendInput
    commitTextViaSendInput(wideText);
}

bool WindowsBridge::commitTextViaTSF(const std::wstring& text) {
    // 检查 TSF 上下文是否可用
    if (!tsfBridge_ || !currentContext_) {
        return false;
    }
    
    // 通过 TSFBridge 提交文本
    // TSFBridge::commitText 会处理 composition 的结束和文本插入
    HRESULT hr = tsfBridge_->commitText(text);
    return SUCCEEDED(hr);
}

void WindowsBridge::commitTextViaSendInput(const std::wstring& text) {
    // 使用 SendInput 模拟 Unicode 字符输入
    // 这是不支持 TSF 的应用的回退方案
    
    for (wchar_t ch : text) {
        INPUT input[2] = {};
        
        // Key down - 使用 UNICODE 标志发送字符
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = 0;
        input[0].ki.wScan = ch;
        input[0].ki.dwFlags = KEYEVENTF_UNICODE;
        input[0].ki.time = 0;
        input[0].ki.dwExtraInfo = 0;
        
        // Key up
        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = 0;
        input[1].ki.wScan = ch;
        input[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        input[1].ki.time = 0;
        input[1].ki.dwExtraInfo = 0;
        
        SendInput(2, input, sizeof(INPUT));
    }
}

// ============================================
// preedit 相关方法实现
// Requirements: 4.1, 4.2, 4.3
// ============================================

void WindowsBridge::updatePreedit(const std::string& preedit, int caretPos) {
    if (!tsfBridge_) {
        return;
    }
    
    std::wstring widePreedit = utf8ToWide(preedit);
    
    // 通过 TSFBridge 更新 preedit
    // TSFBridge 会处理 composition 的创建和更新
    tsfBridge_->updatePreedit(widePreedit, caretPos);
}

void WindowsBridge::clearPreedit() {
    if (!tsfBridge_) {
        return;
    }
    
    // 通过 TSFBridge 清除 preedit
    tsfBridge_->clearPreedit();
}

// ============================================
// getCursorPosition 实现
// Requirements: 5.2, 5.3, 5.4
// ============================================

CursorPosition WindowsBridge::getCursorPosition() {
    CursorPosition pos;
    CaretRect rect;
    
    // 方法1: 使用保存的最后有效 composition 位置
    if (lastCompositionRect_.left != 0 || lastCompositionRect_.top != 0 ||
        lastCompositionRect_.right != 0 || lastCompositionRect_.bottom != 0) {
        pos.x = lastCompositionRect_.left;
        pos.y = lastCompositionRect_.bottom;
        pos.height = lastCompositionRect_.bottom - lastCompositionRect_.top;
        if (pos.height <= 0) pos.height = 20;
        return pos;
    }
    
    // 方法2: 通过 GetGUIThreadInfo 获取光标位置
    if (getCursorRectFromGUIThread(rect)) {
        pos.x = rect.x();
        pos.y = rect.y();
        pos.height = rect.height();
        return pos;
    }
    
    // 方法3: 回退到 GetCaretPos
    if (getCursorRectFromCaret(rect)) {
        pos.x = rect.x();
        pos.y = rect.y();
        pos.height = rect.height();
        return pos;
    }
    
    // 方法4: 回退到鼠标位置
    if (getCursorRectFromMousePos(rect)) {
        pos.x = rect.x();
        pos.y = rect.y();
        pos.height = rect.height();
        return pos;
    }
    
    // 所有方法都失败，返回默认位置
    pos.x = 0;
    pos.y = 0;
    pos.height = 20;
    return pos;
}


bool WindowsBridge::getCursorRectFromTSF(CaretRect& rect) {
    // TSF 方式需要在编辑会话中调用，当前架构不支持
    return false;
}

bool WindowsBridge::getCursorRectFromGUIThread(CaretRect& rect) {
    // 获取前台窗口的线程 ID
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return false;
    }
    
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    if (threadId == 0) {
        return false;
    }
    
    // 使用 GetGUIThreadInfo 获取光标信息
    GUITHREADINFO gti;
    gti.cbSize = sizeof(GUITHREADINFO);
    
    if (!GetGUIThreadInfo(threadId, &gti)) {
        return false;
    }
    
    // 检查是否有有效的光标矩形
    // 注意：某些应用（如 Electron）可能不设置 GUI_CARETBLINKING 标志
    // 所以我们只检查矩形是否有效
    if (gti.rcCaret.left == 0 && gti.rcCaret.top == 0 &&
        gti.rcCaret.right == 0 && gti.rcCaret.bottom == 0) {
        // 矩形全为 0，检查是否有焦点窗口
        if (!gti.hwndFocus) {
            return false;
        }
        // 有焦点窗口但没有光标信息，返回失败让其他方法处理
        return false;
    }
    
    // rcCaret 是相对于 hwndCaret 的客户区坐标
    HWND caretWnd = gti.hwndCaret;
    if (!caretWnd) {
        caretWnd = gti.hwndFocus;
    }
    if (!caretWnd) {
        caretWnd = hwnd;
    }
    
    // 转换为屏幕坐标
    POINT topLeft = { gti.rcCaret.left, gti.rcCaret.top };
    POINT bottomRight = { gti.rcCaret.right, gti.rcCaret.bottom };
    
    ClientToScreen(caretWnd, &topLeft);
    ClientToScreen(caretWnd, &bottomRight);
    
    rect.left = topLeft.x;
    rect.top = topLeft.y;
    rect.right = bottomRight.x;
    rect.bottom = bottomRight.y;
    
    // 确保有合理的高度
    if (rect.bottom <= rect.top) {
        rect.bottom = rect.top + 20;
    }
    
    return true;
}

bool WindowsBridge::getCursorRectFromCaret(CaretRect& rect) {
    // 获取前台窗口
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return false;
    }
    
    // 获取光标位置（客户区坐标）
    POINT caretPos;
    if (!GetCaretPos(&caretPos)) {
        return false;
    }
    
    // 转换为屏幕坐标
    if (!ClientToScreen(hwnd, &caretPos)) {
        return false;
    }
    
    // 设置矩形（假设默认光标高度为 20 像素）
    rect.left = caretPos.x;
    rect.top = caretPos.y;
    rect.right = caretPos.x + 2;  // 光标宽度
    rect.bottom = caretPos.y + 20; // 默认高度
    
    return true;
}

// ============================================
// getCurrentAppId 实现
// Requirements: 10.2, 10.3
// ============================================

std::string WindowsBridge::getCurrentAppId() {
    return getForegroundProcessName();
}

std::string WindowsBridge::getForegroundProcessName() {
    // 获取前台窗口句柄
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        // Requirements: 10.3 - 处理获取失败返回空字符串
        return std::string();
    }
    
    // 获取窗口所属进程 ID
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return std::string();
    }
    
    // 打开进程以查询信息
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return std::string();
    }
    
    // 获取进程完整路径
    wchar_t processPath[MAX_PATH] = {};
    DWORD pathLen = MAX_PATH;
    
    if (QueryFullProcessImageNameW(hProcess, 0, processPath, &pathLen)) {
        CloseHandle(hProcess);
        
        // 提取文件名（不含路径）
        std::wstring fullPath(processPath);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            return wideToUtf8(fullPath.substr(lastSlash + 1));
        }
        return wideToUtf8(fullPath);
    }
    
    CloseHandle(hProcess);
    return std::string();
}

bool WindowsBridge::getCursorRectFromMousePos(CaretRect& rect) {
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return false;
    }
    
    // 使用鼠标位置作为光标位置
    // 在鼠标位置下方显示候选窗口
    rect.left = pt.x;
    rect.top = pt.y;
    rect.right = pt.x + 2;
    rect.bottom = pt.y + 20;
    
    return true;
}

} // namespace suyan

#endif // _WIN32

