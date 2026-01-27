/**
 * CandidateWindow Windows 特定实现
 *
 * 使用 Windows API 设置窗口属性，确保候选词窗口
 * 在所有应用窗口之上显示，包括全屏应用。
 *
 * 关键技术点：
 * 1. 窗口置顶：使用 HWND_TOPMOST 确保在所有窗口之上
 * 2. 焦点控制：设置 WS_EX_NOACTIVATE 防止获取焦点
 * 3. 全屏支持：检测全屏应用并确保窗口可见
 */

#include "candidate_window.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace suyan {

/**
 * 检测指定窗口是否为全屏窗口
 */
static bool isWindowFullScreen(HWND hwnd) {
    if (!hwnd || !IsWindowVisible(hwnd)) {
        return false;
    }
    
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return false;
    }
    
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        return false;
    }
    
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return false;
    }
    
    return (windowRect.left <= monitorInfo.rcMonitor.left &&
            windowRect.top <= monitorInfo.rcMonitor.top &&
            windowRect.right >= monitorInfo.rcMonitor.right &&
            windowRect.bottom >= monitorInfo.rcMonitor.bottom);
}

/**
 * 检测是否有全屏应用正在运行
 */
static bool isFullScreenAppRunning() {
    HWND foreground = GetForegroundWindow();
    return isWindowFullScreen(foreground);
}

/**
 * 设置 Windows 窗口属性
 *
 * 配置窗口以满足输入法候选词窗口的特殊需求：
 * 1. 显示在所有普通窗口之上（包括全屏应用）
 * 2. 不获取键盘焦点
 * 3. 不在任务栏显示
 */
void CandidateWindow::setupWindowsWindowLevel() {
    WId winId = this->winId();
    if (winId == 0) {
        return;
    }
    
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (!hwnd) {
        return;
    }
    
    // 获取当前扩展样式
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    
    // 设置扩展样式：
    // - WS_EX_NOACTIVATE: 窗口不会被激活（关键！）
    // - WS_EX_TOPMOST: 置顶窗口
    // - WS_EX_TOOLWINDOW: 工具窗口，不在任务栏显示
    exStyle |= WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    
    // 设置窗口为置顶
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    // 禁用 DWM 过渡动画
    BOOL disableTransitions = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED,
                          &disableTransitions, sizeof(disableTransitions));
}

/**
 * 确保窗口在全屏应用中正确显示
 */
void CandidateWindow::ensureVisibleInFullScreen() {
    WId winId = this->winId();
    if (winId == 0) {
        return;
    }
    
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (!hwnd) {
        return;
    }
    
    // 重新设置置顶状态
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    // 如果有全屏应用，强制刷新窗口
    if (isFullScreenAppRunning()) {
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    }
}

} // namespace suyan

#endif // Q_OS_WIN
