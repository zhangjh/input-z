/**
 * ClipboardList - 剪贴板历史列表视图
 *
 * 封装 QListWidget，实现剪贴板历史记录的列表展示。
 * 支持滚动懒加载、关键词过滤、记录选择。
 * 
 * 性能优化：
 * - 虚拟化渲染：仅渲染可见区域的 widget
 * - 懒加载：滚动时按需加载更多记录
 * - 延迟缩略图加载：滚动停止后才加载图片
 *
 * Requirements: 5.2, 6.1-6.5
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_LIST_H
#define SUYAN_CLIPBOARD_CLIPBOARD_LIST_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QString>
#include <QTimer>
#include <QSet>
#include <vector>
#include <cstdint>

#include "clipboard_store.h"
#include "clipboard_item_widget.h"

namespace suyan {

/**
 * ClipboardList - 剪贴板历史列表视图
 *
 * 封装 QListWidget，提供剪贴板历史记录的列表展示功能。
 * 实现虚拟化渲染以优化大数据量下的滚动性能。
 */
class ClipboardList : public QWidget {
    Q_OBJECT

public:
    /**
     * 构造函数
     *
     * @param parent 父组件
     */
    explicit ClipboardList(QWidget* parent = nullptr);
    ~ClipboardList() override = default;

    /**
     * 加载历史记录
     *
     * 清空当前列表，从数据库加载记录并创建 ClipboardItemWidget。
     * 初始加载 PAGE_SIZE 条记录，后续通过滚动懒加载。
     */
    void loadRecords();

    /**
     * 根据关键词过滤显示
     *
     * @param keyword 搜索关键词，空字符串显示全部
     */
    void filterByKeyword(const QString& keyword);

    /**
     * 刷新列表
     *
     * 重新加载所有记录。
     */
    void refresh();

    /**
     * 清空列表
     */
    void clear();

    /**
     * 获取当前记录数量
     */
    int recordCount() const;

    /**
     * 获取当前选中的记录 ID
     *
     * @return 选中的记录 ID，无选中返回 -1
     */
    int64_t selectedRecordId() const;

    /**
     * 选中指定记录
     *
     * @param recordId 记录 ID
     */
    void selectRecord(int64_t recordId);

    /**
     * 更新所有项的时间戳显示
     */
    void updateTimestamps();

signals:
    /**
     * 记录被选中信号
     *
     * @param recordId 被选中的记录 ID
     */
    void itemSelected(int64_t recordId);

    /**
     * 列表加载完成信号
     *
     * @param count 加载的记录数量
     */
    void loadCompleted(int count);

    /**
     * 列表为空信号
     */
    void listEmpty();

private slots:
    /**
     * 处理滚动事件，实现懒加载
     *
     * @param value 滚动条当前值
     */
    void onScrollValueChanged(int value);

    /**
     * 处理列表项点击
     *
     * @param recordId 被点击的记录 ID
     */
    void onItemClicked(int64_t recordId);

    /**
     * 更新可见区域的 widget（虚拟化渲染）
     */
    void updateVisibleWidgets();

    /**
     * 滚动停止后的处理（延迟加载缩略图等）
     */
    void onScrollingStopped();

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
     * 加载更多记录（懒加载）
     */
    void loadMoreRecords();

    /**
     * 添加记录到列表（仅创建占位项，不创建 widget）
     *
     * @param record 剪贴板记录
     */
    void addRecordToList(const ClipboardRecord& record);

    /**
     * 为指定索引创建或获取 widget
     *
     * @param index 列表项索引
     * @return 对应的 widget，如果记录不存在返回 nullptr
     */
    ClipboardItemWidget* ensureWidgetForIndex(int index);

    /**
     * 回收不可见的 widget
     */
    void recycleInvisibleWidgets();

    /**
     * 获取当前可见的列表项索引范围
     *
     * @param firstVisible 输出：第一个可见项索引
     * @param lastVisible 输出：最后一个可见项索引
     */
    void getVisibleRange(int& firstVisible, int& lastVisible) const;

    /**
     * 更新空列表提示
     */
    void updateEmptyHint();

    /**
     * 显示/隐藏空列表提示
     *
     * @param show 是否显示
     * @param message 提示消息
     */
    void showEmptyHint(bool show, const QString& message = QString());

    // UI 组件
    QVBoxLayout* mainLayout_ = nullptr;
    QListWidget* listWidget_ = nullptr;
    QLabel* emptyHintLabel_ = nullptr;

    // 数据
    std::vector<ClipboardRecord> allRecords_;       // 所有已加载的记录
    QString currentKeyword_;                         // 当前过滤关键词
    int64_t selectedRecordId_ = -1;                 // 当前选中的记录 ID

    // 虚拟化渲染状态
    QSet<int> renderedIndices_;                     // 已渲染 widget 的索引
    QTimer* scrollStopTimer_ = nullptr;            // 滚动停止检测定时器
    int lastScrollValue_ = 0;                       // 上次滚动位置

    // 懒加载状态
    int loadedCount_ = 0;                           // 已加载的记录数量
    bool isLoading_ = false;                        // 是否正在加载
    bool hasMoreRecords_ = true;                    // 是否还有更多记录
    bool isFiltering_ = false;                      // 是否处于过滤模式

    // 常量
    static constexpr int PAGE_SIZE = 50;            // 每次加载的记录数量
    static constexpr int SCROLL_THRESHOLD = 100;   // 触发加载的滚动阈值（像素）
    static constexpr int VISIBLE_BUFFER = 5;       // 可见区域外额外渲染的项数
    static constexpr int MAX_RENDERED_WIDGETS = 30; // 最大同时渲染的 widget 数量
    static constexpr int SCROLL_STOP_DELAY_MS = 100; // 滚动停止检测延迟
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_LIST_H
