/**
 * CandidateWindow - 候选词窗口
 *
 * 负责管理候选词窗口的显示、隐藏、定位。
 * 实现无边框置顶窗口，支持跟随光标位置，处理屏幕边缘情况。
 */

#ifndef SUYAN_UI_CANDIDATE_WINDOW_H
#define SUYAN_UI_CANDIDATE_WINDOW_H

#include <QWidget>
#include <QPoint>
#include <QScreen>
#include "candidate_view.h"
#include "theme_manager.h"
#include "layout_manager.h"

#include <QResizeEvent>
#ifdef _WIN32
#include <windows.h>
#endif

namespace suyan {

// 前向声明
struct InputState;

/**
 * CandidateWindow - 候选词窗口类
 *
 * 职责：
 * 1. 管理无边框置顶窗口
 * 2. 定位窗口到光标附近
 * 3. 处理屏幕边缘情况
 * 4. 集成 CandidateView 进行渲染
 * 5. 响应主题和布局变化
 */
class CandidateWindow : public QWidget {
    Q_OBJECT

public:
    explicit CandidateWindow(QWidget* parent = nullptr);
    ~CandidateWindow() override;

    // ========== 候选词更新 ==========

    /**
     * 更新候选词显示
     *
     * @param state 输入状态
     */
    void updateCandidates(const InputState& state);

    /**
     * 清空候选词
     */
    void clearCandidates();

    // ========== 窗口显示控制 ==========

    /**
     * 在指定位置显示窗口
     * @param cursorPos 光标位置（屏幕坐标）
     */
    void showAt(const QPoint& cursorPos);

    /**
     * 在指定位置显示窗口 (Native Coordinates/Boundary Aware)
     * 
     * 直接使用 Windows API SetWindowPos 设置位置，绕过 Qt 的 DPI 映射。
     * 自动处理屏幕边界溢出：
     * - 如果右侧空间不足，向左对齐
     * - 如果下方空间不足，显示在上方
     * 
     * @param cursorRect 光标矩形 (Physical/Screen Coordinates)
     */
    void showAtNative(const QRect& cursorRect);

    /**
     * 隐藏窗口
     */
    void hideWindow();

    /**
     * 检查窗口是否可见
     */
    bool isWindowVisible() const;

    // ========== 布局设置 ==========

    /**
     * 设置布局类型
     *
     * @param type 布局类型（横排/竖排）
     */
    void setLayoutType(LayoutType type);

    /**
     * 获取当前布局类型
     */
    LayoutType getLayoutType() const;

    // ========== 主题设置 ==========

    /**
     * 设置主题
     *
     * @param theme 主题配置
     */
    void setTheme(const Theme& theme);

    /**
     * 获取当前主题
     */
    const Theme& getTheme() const;

    // ========== 窗口定位 ==========

    /**
     * 设置光标位置偏移
     *
     * @param offset 偏移量（窗口相对于光标的偏移）
     */
    void setCursorOffset(const QPoint& offset);

    /**
     * 获取光标位置偏移
     */
    QPoint getCursorOffset() const { return cursorOffset_; }

    /**
     * 更新窗口位置（使用上次的光标位置）
     */
    void updatePosition();

    // ========== 访问内部视图 ==========

    /**
     * 获取候选词视图
     */
    CandidateView* candidateView() const { return candidateView_; }

    // ========== 管理器连接 ==========

    /**
     * 连接 ThemeManager 信号
     * 
     * 当 ThemeManager 的主题变化时，自动更新窗口主题
     */
    void connectToThemeManager();

    /**
     * 连接 LayoutManager 信号
     * 
     * 当 LayoutManager 的布局变化时，自动更新窗口布局
     */
    void connectToLayoutManager();

    /**
     * 断开 ThemeManager 信号连接
     */
    void disconnectFromThemeManager();

    /**
     * 断开 LayoutManager 信号连接
     */
    void disconnectFromLayoutManager();

    /**
     * 从管理器同步当前配置
     * 
     * 从 ThemeManager 和 LayoutManager 获取当前配置并应用
     */
    void syncFromManagers();

signals:
    /**
     * 候选词被点击
     *
     * @param index 候选词索引 (0-based)
     */
    void candidateClicked(int index);

    /**
     * 窗口显示状态变化
     *
     * @param visible 是否可见
     */
    void visibilityChanged(bool visible);

protected:
    /**
     * 重写显示事件
     */
    void showEvent(QShowEvent* event) override;

    /**
     * 重写隐藏事件
     */
    void hideEvent(QHideEvent* event) override;

    /**
     * 重写大小变化事件
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    // 初始化窗口属性
    void initWindowFlags();

    // 设置 macOS 特定的窗口级别
    void setupMacOSWindowLevel();
    
    // 设置 Windows 特定的窗口级别
    void setupWindowsWindowLevel();
    
    // 确保窗口在全屏应用中可见
    void ensureVisibleInFullScreen();

    // 计算窗口位置（处理屏幕边缘）
    QPoint calculateWindowPosition(const QPoint& cursorPos, const QSize& windowSize);

    // 获取光标所在的屏幕
    QScreen* getScreenAtCursor(const QPoint& cursorPos);
    
    // Native implementation
    void showAtNativeImpl(const QPoint& pos);

    // 成员变量
    CandidateView* candidateView_ = nullptr;
    QPoint lastCursorPos_;
    QPoint cursorOffset_{0, 0};  // 默认偏移为0 (由 TSFBridge 控制实际位置)
    bool positionInitialized_ = false;

    // 信号连接
    QMetaObject::Connection themeConnection_;
    QMetaObject::Connection layoutConnection_;
    QMetaObject::Connection pageSizeConnection_;
};

} // namespace suyan

#endif // SUYAN_UI_CANDIDATE_WINDOW_H
