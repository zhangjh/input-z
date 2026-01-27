/**
 * CandidateWindow 实现
 */

#include "candidate_window.h"
#include "../core/input_engine.h"
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>

namespace suyan {

// ========== 构造/析构 ==========

CandidateWindow::CandidateWindow(QWidget* parent)
    : QWidget(parent)
{
    // 初始化窗口标志
    initWindowFlags();
    
    // 创建候选词视图
    candidateView_ = new CandidateView(this);
    
    // 设置布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(candidateView_);
    
    // 连接候选词点击信号
    connect(candidateView_, &CandidateView::candidateClicked,
            this, &CandidateWindow::candidateClicked);
    
    // 设置平台特定的窗口级别
#ifdef Q_OS_MACOS
    setupMacOSWindowLevel();
#elif defined(Q_OS_WIN)
    setupWindowsWindowLevel();
#endif
    
    // 连接 ThemeManager 和 LayoutManager
    connectToThemeManager();
    connectToLayoutManager();
    
    // 从管理器同步当前配置
    syncFromManagers();
    
    // 初始隐藏
    hide();
}

CandidateWindow::~CandidateWindow() {
    // 断开信号连接
    disconnectFromThemeManager();
    disconnectFromLayoutManager();
}

// ========== 窗口初始化 ==========

void CandidateWindow::initWindowFlags() {
    // 设置窗口标志：
    // - Qt::Tool: 工具窗口，不在任务栏显示
    // - Qt::FramelessWindowHint: 无边框
    // - Qt::WindowStaysOnTopHint: 置顶
    // - Qt::WindowDoesNotAcceptFocus: 不获取焦点（关键！）
    // - Qt::BypassWindowManagerHint: 绕过窗口管理器（某些平台需要）
    setWindowFlags(Qt::Tool 
                   | Qt::FramelessWindowHint 
                   | Qt::WindowStaysOnTopHint 
                   | Qt::WindowDoesNotAcceptFocus
                   | Qt::BypassWindowManagerHint);
    
    // 设置窗口属性
    setAttribute(Qt::WA_ShowWithoutActivating);      // 显示时不激活（关键！）
    setAttribute(Qt::WA_TranslucentBackground);      // 透明背景
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);    // macOS: 始终显示工具窗口
    setAttribute(Qt::WA_X11DoNotAcceptFocus);        // X11: 不接受焦点（Linux 兼容）
    
    // 禁用输入法（候选词窗口本身不需要输入法）
    setAttribute(Qt::WA_InputMethodEnabled, false);
}

// ========== 候选词更新 ==========

void CandidateWindow::updateCandidates(const InputState& state) {
    // 更新候选词视图
    candidateView_->updateFromState(state);
    
    // 如果不在输入状态，隐藏窗口
    if (!state.isComposing) {
        hideWindow();
        return;
    }
    
    // 如果有 preedit 或候选词，显示窗口
    if (!state.preedit.empty() || !state.candidates.empty()) {
        // 调整窗口大小
        QSize newSize = candidateView_->sizeHint();
        resize(newSize);
        
        // 如果窗口已定位，更新位置
        if (positionInitialized_) {
            updatePosition();
        }
    } else {
        // 没有 preedit 也没有候选词，隐藏窗口
        hideWindow();
    }
}

void CandidateWindow::clearCandidates() {
    candidateView_->setCandidates({});
    candidateView_->setPreedit(QString());
    hideWindow();
}

// ========== 窗口显示控制 ==========

void CandidateWindow::showAt(const QPoint& cursorPos) {
#ifndef Q_OS_WIN
    lastCursorPos_ = cursorPos;
    positionInitialized_ = true;
    
    // 计算窗口位置
    QSize windowSize = candidateView_->sizeHint();
    resize(windowSize);
    
    QPoint windowPos = calculateWindowPosition(cursorPos, windowSize);
    move(windowPos);
    
    // 显示窗口
    if (!isVisible()) {
        show();
    }
    
    // 确保在全屏应用中可见
#if defined(Q_OS_MACOS)
    ensureVisibleInFullScreen();
#endif

#else
    // Windows implementation is handled by showAtNative()
    // This is kept as stub to satisfy linker/interface
    Q_UNUSED(cursorPos);
#endif
}

void CandidateWindow::hideWindow() {
    if (isVisible()) {
        hide();
    }
}

bool CandidateWindow::isWindowVisible() const {
    return isVisible();
}

// ========== 布局设置 ==========

void CandidateWindow::setLayoutType(LayoutType type) {
    candidateView_->setLayoutType(type);
    
    // 调整窗口大小
    QSize newSize = candidateView_->sizeHint();
    resize(newSize);
    
    // 更新位置
    if (positionInitialized_) {
        updatePosition();
    }
}

LayoutType CandidateWindow::getLayoutType() const {
    return candidateView_->getLayoutType();
}

// ========== 主题设置 ==========

void CandidateWindow::setTheme(const Theme& theme) {
    candidateView_->setTheme(theme);
    
    // 调整窗口大小
    QSize newSize = candidateView_->sizeHint();
    resize(newSize);
    
    // 更新位置
    if (positionInitialized_) {
        updatePosition();
    }
}

const Theme& CandidateWindow::getTheme() const {
    return candidateView_->getTheme();
}

// ========== 窗口定位 ==========

void CandidateWindow::setCursorOffset(const QPoint& offset) {
    cursorOffset_ = offset;
}

void CandidateWindow::updatePosition() {
    if (!positionInitialized_) {
        return;
    }
    
    QSize windowSize = size();
    QPoint windowPos = calculateWindowPosition(lastCursorPos_, windowSize);
    move(windowPos);
}



void CandidateWindow::showAtNative(const QRect& cursorRect) {
#ifdef _WIN32
    if (!isVisible()) {
        QWidget::setVisible(true);
    }
    adjustSize();
    
    HWND hwnd = (HWND)winId();
    if (hwnd) {
        RECT winRect;
        GetWindowRect(hwnd, &winRect);
        int w = winRect.right - winRect.left;
        int h = winRect.bottom - winRect.top;
        
        POINT targetPt = { cursorRect.left(), cursorRect.bottom() };
        HMONITOR hMon = MonitorFromPoint(targetPt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(hMon, &mi);
        
        int x = cursorRect.left();
        int y = cursorRect.bottom() + 2;
        
        if (x + w > mi.rcWork.right) {
            x = mi.rcWork.right - w;
            if (x < mi.rcWork.left) {
                x = mi.rcWork.left;
            }
        }
        
        if (y + h > mi.rcWork.bottom) {
            int topY = cursorRect.top() - h - 5;
            if (topY >= mi.rcWork.top) {
                y = topY;
            } else {
                y = mi.rcWork.bottom - h;
            }
        }
        
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, 
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        
        lastCursorPos_ = QPoint(x, y);
        positionInitialized_ = true;
    }
#else
    showAt(cursorRect.bottomLeft());
#endif
}

QPoint CandidateWindow::calculateWindowPosition(const QPoint& cursorPos, 
                                                 const QSize& windowSize) {
    // 获取光标所在屏幕
    QScreen* screen = getScreenAtCursor(cursorPos);
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    
    QRect screenGeometry = screen->availableGeometry();
    
    // 计算初始位置（光标下方）
    int x = cursorPos.x() + cursorOffset_.x();
    int y = cursorPos.y() + cursorOffset_.y();
    
    // 处理右边缘：如果窗口超出屏幕右边，向左移动
    if (x + windowSize.width() > screenGeometry.right()) {
        x = screenGeometry.right() - windowSize.width();
    }
    
    // 处理左边缘：确保不超出屏幕左边
    if (x < screenGeometry.left()) {
        x = screenGeometry.left();
    }
    
    // 处理下边缘：如果窗口超出屏幕下边，显示在光标上方
    if (y + windowSize.height() > screenGeometry.bottom()) {
        // 在光标上方显示
        y = cursorPos.y() - windowSize.height() - 5;
    }
    
    // 处理上边缘：确保不超出屏幕上边
    if (y < screenGeometry.top()) {
        y = screenGeometry.top();
    }
    
    return QPoint(x, y);
}

QScreen* CandidateWindow::getScreenAtCursor(const QPoint& cursorPos) {
    // 遍历所有屏幕，找到包含光标的屏幕
    const QList<QScreen*> screens = QApplication::screens();
    for (QScreen* screen : screens) {
        if (screen->geometry().contains(cursorPos)) {
            return screen;
        }
    }
    return nullptr;
}

// ========== 事件处理 ==========

void CandidateWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void CandidateWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void CandidateWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    // 窗口大小变化后，重新计算位置
    if (positionInitialized_ && isVisible()) {
        updatePosition();
    }
}

// 非 macOS/Windows 平台的空实现
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
void CandidateWindow::setupMacOSWindowLevel() {
}

void CandidateWindow::setupWindowsWindowLevel() {
}

void CandidateWindow::ensureVisibleInFullScreen() {
}
#elif defined(Q_OS_WIN)
void CandidateWindow::setupMacOSWindowLevel() {
}
#elif defined(Q_OS_MACOS)
void CandidateWindow::setupWindowsWindowLevel() {
}
#endif

// ========== 管理器连接 ==========

void CandidateWindow::connectToThemeManager() {
    // 断开旧连接（如果有）
    disconnectFromThemeManager();
    
    // 连接 ThemeManager 的主题变更信号
    themeConnection_ = connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
                               this, [this](const Theme& theme) {
        setTheme(theme);
    });
}

void CandidateWindow::connectToLayoutManager() {
    // 断开旧连接（如果有）
    disconnectFromLayoutManager();
    
    // 连接 LayoutManager 的布局类型变更信号
    layoutConnection_ = connect(&LayoutManager::instance(), &LayoutManager::layoutTypeChanged,
                                this, [this](LayoutType type) {
        setLayoutType(type);
    });
    
    // 连接 LayoutManager 的每页候选词数量变更信号（可选，用于未来扩展）
    pageSizeConnection_ = connect(&LayoutManager::instance(), &LayoutManager::pageSizeChanged,
                                  this, [this](int /*size*/) {
        // 每页候选词数量变化时，可能需要更新显示
        // 目前由 InputEngine 控制，这里预留接口
        if (positionInitialized_) {
            updatePosition();
        }
    });
}

void CandidateWindow::disconnectFromThemeManager() {
    if (themeConnection_) {
        disconnect(themeConnection_);
        themeConnection_ = QMetaObject::Connection();
    }
}

void CandidateWindow::disconnectFromLayoutManager() {
    if (layoutConnection_) {
        disconnect(layoutConnection_);
        layoutConnection_ = QMetaObject::Connection();
    }
    if (pageSizeConnection_) {
        disconnect(pageSizeConnection_);
        pageSizeConnection_ = QMetaObject::Connection();
    }
}

void CandidateWindow::syncFromManagers() {
    // 从 ThemeManager 同步主题
    if (ThemeManager::instance().isInitialized()) {
        setTheme(ThemeManager::instance().getCurrentTheme());
    }
    
    // 从 LayoutManager 同步布局
    if (LayoutManager::instance().isInitialized()) {
        setLayoutType(LayoutManager::instance().getLayoutType());
    }
}

} // namespace suyan
