/**
 * ClipboardWindow 实现
 *
 * 性能优化：
 * - 延迟初始化：首次显示时才加载数据
 * - 预创建 UI：构造时创建 UI，避免显示时延迟
 * - 异步加载：使用 QTimer::singleShot 延迟加载数据，不阻塞窗口显示
 *
 * Requirements: 5.1-5.9, 6.1-6.5
 */

#include "clipboard_window.h"
#include "clipboard_list.h"

#include <QApplication>
#include <QScreen>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QFrame>

namespace suyan {

ClipboardWindow::ClipboardWindow(QWidget* parent)
    : QWidget(parent)
{
    setupWindowAttributes();
    setupUI();
    connectSignals();
}

ClipboardWindow::~ClipboardWindow()
{
    // 移除事件过滤器
    qApp->removeEventFilter(this);
}

void ClipboardWindow::setupWindowAttributes()
{
    // 设置窗口标志：工具窗口、无边框、置顶
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // 设置 WA_ShowWithoutActivating，窗口显示时不自动获取焦点
    // 但在 showWindow() 中我们会手动激活窗口以便搜索框可以输入
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground, false);

    // 设置固定大小
    setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
}

void ClipboardWindow::setupUI()
{
    // 主布局
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(8, 8, 8, 8);
    mainLayout_->setSpacing(8);

    // 设置窗口背景样式
    setStyleSheet(
        "ClipboardWindow {"
        "    background-color: #ffffff;"
        "    border: 1px solid #e0e0e0;"
        "    border-radius: 8px;"
        "}"
    );

    // 搜索框
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("搜索剪贴板历史...");
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setStyleSheet(
        "QLineEdit {"
        "    padding: 8px 12px;"
        "    border: 1px solid #e0e0e0;"
        "    border-radius: 6px;"
        "    background-color: #f5f5f5;"
        "    font-size: 14px;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #007AFF;"
        "    background-color: #ffffff;"
        "}"
    );
    mainLayout_->addWidget(searchEdit_);

    // 剪贴板列表
    clipboardList_ = new ClipboardList(this);
    mainLayout_->addWidget(clipboardList_, 1);  // stretch factor = 1

    // 搜索防抖定时器
    searchDebounceTimer_ = new QTimer(this);
    searchDebounceTimer_->setSingleShot(true);
}

void ClipboardWindow::connectSignals()
{
    // 搜索框文本变化 -> 防抖处理
    connect(searchEdit_, &QLineEdit::textChanged,
            this, &ClipboardWindow::onSearchTextChanged);

    // 防抖定时器超时 -> 执行搜索
    connect(searchDebounceTimer_, &QTimer::timeout,
            this, &ClipboardWindow::performSearch);

    // 列表项选中 -> 转发信号
    connect(clipboardList_, &ClipboardList::itemSelected,
            this, &ClipboardWindow::onItemSelected);
}

void ClipboardWindow::showWindow()
{
    // 清空搜索框（在显示前清空，避免触发搜索）
    searchEdit_->blockSignals(true);
    searchEdit_->clear();
    searchEdit_->blockSignals(false);

    // 定位到屏幕中央
    centerOnScreen();

    // 先显示窗口（优化首次显示延迟）
    show();
    raise();
    activateWindow();

    // 聚焦到搜索框
    searchEdit_->setFocus();

    // 安装全局事件过滤器，用于检测点击窗口外部
    qApp->installEventFilter(this);

    // 延迟加载数据（不阻塞窗口显示）
    if (needsRefresh_ || !dataLoaded_) {
        QTimer::singleShot(0, this, &ClipboardWindow::loadDataDeferred);
    }

    emit windowShown();
}

void ClipboardWindow::loadDataDeferred()
{
    if (clipboardList_) {
        clipboardList_->loadRecords();
        dataLoaded_ = true;
        needsRefresh_ = false;
    }
}

void ClipboardWindow::hideWindow()
{
    // 移除事件过滤器
    qApp->removeEventFilter(this);

    hide();
    emit windowHidden();
}

void ClipboardWindow::toggleVisibility()
{
    if (isVisible()) {
        hideWindow();
    } else {
        showWindow();
    }
}

void ClipboardWindow::refreshList()
{
    // 标记需要刷新
    needsRefresh_ = true;
    
    // 如果窗口可见，立即刷新
    if (isVisible() && clipboardList_) {
        clipboardList_->refresh();
        needsRefresh_ = false;
    }
}

void ClipboardWindow::setSearchKeyword(const QString& keyword)
{
    if (searchEdit_) {
        searchEdit_->setText(keyword);
    }
}

void ClipboardWindow::clearSearch()
{
    if (searchEdit_) {
        searchEdit_->clear();
    }
}

bool ClipboardWindow::isWindowVisible() const
{
    return isVisible();
}

void ClipboardWindow::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    // 注意：focusOutEvent 在某些情况下可能不会触发
    // 我们主要依赖 eventFilter 来处理点击外部隐藏
    QWidget::focusOutEvent(event);
}

void ClipboardWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hideWindow();
        event->accept();
        return;
    }

    // 处理上下键导航（可选功能）
    if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) {
        // 将键盘事件传递给列表
        if (clipboardList_) {
            QApplication::sendEvent(clipboardList_, event);
            return;
        }
    }

    // 处理回车键选择当前项
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (clipboardList_) {
            int64_t selectedId = clipboardList_->selectedRecordId();
            if (selectedId >= 0) {
                onItemSelected(selectedId);
                return;
            }
        }
    }

    QWidget::keyPressEvent(event);
}

void ClipboardWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

void ClipboardWindow::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
}

bool ClipboardWindow::eventFilter(QObject* watched, QEvent* event)
{
    // 检测鼠标点击事件
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        // 获取点击的全局位置
        QPoint globalPos = mouseEvent->globalPosition().toPoint();

        // 检查点击是否在窗口外部
        QRect windowRect = geometry();
        if (!windowRect.contains(globalPos)) {
            // 点击在窗口外部，隐藏窗口
            hideWindow();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ClipboardWindow::onSearchTextChanged(const QString& text)
{
    // 保存待搜索文本
    pendingSearchText_ = text;

    // 重启防抖定时器
    searchDebounceTimer_->stop();
    searchDebounceTimer_->start(SEARCH_DEBOUNCE_MS);
}

void ClipboardWindow::performSearch()
{
    if (clipboardList_) {
        clipboardList_->filterByKeyword(pendingSearchText_);
    }
}

void ClipboardWindow::onItemSelected(int64_t recordId)
{
    // 发射记录选中信号
    // 窗口隐藏由 ClipboardManager::pasteCompleted 信号触发
    emit recordSelected(recordId);
}

void ClipboardWindow::centerOnScreen()
{
    // 获取主屏幕
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) {
        return;
    }

    // 获取屏幕可用区域（排除任务栏等）
    QRect screenGeometry = screen->availableGeometry();

    // 计算居中位置
    int x = screenGeometry.x() + (screenGeometry.width() - width()) / 2;
    int y = screenGeometry.y() + (screenGeometry.height() - height()) / 2;

    move(x, y);
}

} // namespace suyan
