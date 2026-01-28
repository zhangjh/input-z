/**
 * ClipboardWindow - 剪贴板历史悬浮窗口
 *
 * 显示剪贴板历史记录的独立悬浮窗口。
 * 支持搜索过滤、记录选择、快捷键隐藏等功能。
 *
 * 性能优化：
 * - 延迟初始化：首次显示时才加载数据
 * - 预创建 UI：构造时创建 UI，避免显示时延迟
 * - 异步加载：数据加载不阻塞 UI 显示
 *
 * Requirements: 5.1-5.9, 6.1-6.5
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_WINDOW_H
#define SUYAN_CLIPBOARD_CLIPBOARD_WINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QTimer>
#include <QString>
#include <QElapsedTimer>
#include <cstdint>

namespace suyan {

// 前向声明
class ClipboardList;

/**
 * ClipboardWindow - 剪贴板历史窗口
 *
 * 悬浮窗口，显示剪贴板历史记录列表，支持搜索过滤。
 * 窗口属性：无边框、置顶、不获取焦点。
 */
class ClipboardWindow : public QWidget {
    Q_OBJECT

public:
    /**
     * 构造函数
     *
     * @param parent 父组件
     */
    explicit ClipboardWindow(QWidget* parent = nullptr);
    ~ClipboardWindow() override;

    /**
     * 显示窗口（屏幕中央）
     */
    void showWindow();

    /**
     * 隐藏窗口
     */
    void hideWindow();

    /**
     * 切换显示状态
     */
    void toggleVisibility();

    /**
     * 刷新列表
     */
    void refreshList();

    /**
     * 设置搜索关键词
     *
     * @param keyword 搜索关键词
     */
    void setSearchKeyword(const QString& keyword);

    /**
     * 清空搜索框
     */
    void clearSearch();

    /**
     * 窗口是否可见
     */
    bool isWindowVisible() const;

signals:
    /**
     * 记录被选中（用于粘贴）
     *
     * @param recordId 被选中的记录 ID
     */
    void recordSelected(int64_t recordId);

    /**
     * 窗口隐藏信号
     */
    void windowHidden();

    /**
     * 窗口显示信号
     */
    void windowShown();

protected:
    /**
     * 失去焦点时隐藏
     */
    void focusOutEvent(QFocusEvent* event) override;

    /**
     * 按键事件处理
     * - Escape: 隐藏窗口
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * 显示事件
     */
    void showEvent(QShowEvent* event) override;

    /**
     * 隐藏事件
     */
    void hideEvent(QHideEvent* event) override;

    /**
     * 事件过滤器
     * 用于处理点击窗口外部区域隐藏
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    /**
     * 搜索框文本变化处理（带防抖）
     */
    void onSearchTextChanged(const QString& text);

    /**
     * 执行搜索过滤
     */
    void performSearch();

    /**
     * 处理列表项选中
     *
     * @param recordId 被选中的记录 ID
     */
    void onItemSelected(int64_t recordId);

    /**
     * 延迟加载数据（用于优化首次显示性能）
     */
    void loadDataDeferred();

private:
    /**
     * 初始化 UI
     */
    void setupUI();

    /**
     * 连接信号槽
     */
    void connectSignals();

    /**
     * 设置窗口属性
     */
    void setupWindowAttributes();

    /**
     * 将窗口定位到屏幕中央
     */
    void centerOnScreen();

    // UI 组件
    QVBoxLayout* mainLayout_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    ClipboardList* clipboardList_ = nullptr;

    // 搜索防抖定时器
    QTimer* searchDebounceTimer_ = nullptr;
    QString pendingSearchText_;

    // 性能优化相关
    bool dataLoaded_ = false;           // 数据是否已加载
    bool needsRefresh_ = true;          // 是否需要刷新数据

    // 窗口尺寸常量
    static constexpr int WINDOW_WIDTH = 400;
    static constexpr int WINDOW_HEIGHT = 500;
    static constexpr int SEARCH_DEBOUNCE_MS = 300;
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_WINDOW_H
