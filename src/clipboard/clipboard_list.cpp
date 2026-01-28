/**
 * ClipboardList 实现
 *
 * 性能优化：
 * - 虚拟化渲染：仅渲染可见区域的 widget，减少内存占用
 * - 懒加载：滚动时按需加载更多记录
 * - 延迟缩略图加载：滚动停止后才加载图片
 * - Widget 回收：超出可见区域的 widget 被回收
 *
 * Requirements: 5.2, 6.1-6.5
 */

#include "clipboard_list.h"
#include "clipboard_manager.h"

#include <QScrollBar>
#include <QListWidgetItem>
#include <QApplication>

namespace suyan {

ClipboardList::ClipboardList(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
}

void ClipboardList::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);

    // 创建列表组件
    listWidget_ = new QListWidget(this);
    listWidget_->setFrameShape(QFrame::NoFrame);
    listWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget_->setSpacing(0);
    
    // 启用统一项高度以优化性能
    listWidget_->setUniformItemSizes(true);

    // 设置样式
    listWidget_->setStyleSheet(
        "QListWidget {"
        "    background-color: white;"
        "    border: none;"
        "}"
        "QListWidget::item {"
        "    border: none;"
        "    padding: 0px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: transparent;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: transparent;"
        "}"
    );

    mainLayout_->addWidget(listWidget_);

    // 创建空列表提示标签
    emptyHintLabel_ = new QLabel(this);
    emptyHintLabel_->setAlignment(Qt::AlignCenter);
    emptyHintLabel_->setStyleSheet(
        "QLabel {"
        "    color: #999999;"
        "    font-size: 14px;"
        "    padding: 40px;"
        "}"
    );
    emptyHintLabel_->hide();
    mainLayout_->addWidget(emptyHintLabel_);

    // 创建滚动停止检测定时器
    scrollStopTimer_ = new QTimer(this);
    scrollStopTimer_->setSingleShot(true);
    connect(scrollStopTimer_, &QTimer::timeout,
            this, &ClipboardList::onScrollingStopped);
}

void ClipboardList::connectSignals()
{
    // 连接滚动条信号，实现懒加载和虚拟化渲染
    connect(listWidget_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ClipboardList::onScrollValueChanged);
}

void ClipboardList::loadRecords()
{
    // 清空当前列表
    clear();

    // 重置状态
    loadedCount_ = 0;
    hasMoreRecords_ = true;
    isFiltering_ = false;
    currentKeyword_.clear();
    allRecords_.clear();
    renderedIndices_.clear();

    // 加载第一批记录
    loadMoreRecords();
}

void ClipboardList::loadMoreRecords()
{
    if (isLoading_ || !hasMoreRecords_ || isFiltering_) {
        return;
    }

    isLoading_ = true;

    // 从数据库加载记录
    auto records = ClipboardStore::instance().getAllRecords(PAGE_SIZE, loadedCount_);

    if (records.empty()) {
        hasMoreRecords_ = false;
        isLoading_ = false;
        updateEmptyHint();
        return;
    }

    // 如果返回的记录数少于 PAGE_SIZE，说明没有更多记录了
    if (static_cast<int>(records.size()) < PAGE_SIZE) {
        hasMoreRecords_ = false;
    }

    // 添加记录到列表（仅创建占位项）
    for (const auto& record : records) {
        addRecordToList(record);
        allRecords_.push_back(record);
    }

    loadedCount_ += static_cast<int>(records.size());
    isLoading_ = false;

    // 更新可见区域的 widget
    updateVisibleWidgets();

    updateEmptyHint();
    emit loadCompleted(loadedCount_);
}

void ClipboardList::addRecordToList(const ClipboardRecord& record)
{
    Q_UNUSED(record);
    
    // 创建列表项（占位，不创建 widget）
    auto* item = new QListWidgetItem(listWidget_);
    
    // 设置固定高度（与 ClipboardItemWidget::WIDGET_HEIGHT 一致）
    item->setSizeHint(QSize(listWidget_->viewport()->width(), 60));
}

ClipboardItemWidget* ClipboardList::ensureWidgetForIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(allRecords_.size())) {
        return nullptr;
    }

    auto* item = listWidget_->item(index);
    if (!item) {
        return nullptr;
    }

    // 检查是否已有 widget
    auto* existingWidget = qobject_cast<ClipboardItemWidget*>(
        listWidget_->itemWidget(item)
    );
    if (existingWidget) {
        return existingWidget;
    }

    // 创建新的 widget
    const auto& record = allRecords_[index];
    auto* widget = new ClipboardItemWidget(record, listWidget_);
    
    // 设置列表项大小
    item->setSizeHint(widget->sizeHint());
    
    // 将 widget 设置到列表项
    listWidget_->setItemWidget(item, widget);
    
    // 连接点击信号
    connect(widget, &ClipboardItemWidget::clicked,
            this, &ClipboardList::onItemClicked);

    // 如果是选中的记录，设置选中状态
    if (record.id == selectedRecordId_) {
        widget->setSelected(true);
    }

    renderedIndices_.insert(index);
    return widget;
}

void ClipboardList::recycleInvisibleWidgets()
{
    if (renderedIndices_.size() <= MAX_RENDERED_WIDGETS) {
        return;
    }

    int firstVisible, lastVisible;
    getVisibleRange(firstVisible, lastVisible);

    // 扩展可见范围（保留缓冲区）
    int keepStart = qMax(0, firstVisible - VISIBLE_BUFFER);
    int keepEnd = qMin(listWidget_->count() - 1, lastVisible + VISIBLE_BUFFER);

    // 收集需要回收的索引
    QList<int> toRecycle;
    for (int index : renderedIndices_) {
        if (index < keepStart || index > keepEnd) {
            toRecycle.append(index);
        }
    }

    // 回收 widget
    for (int index : toRecycle) {
        auto* item = listWidget_->item(index);
        if (item) {
            auto* widget = listWidget_->itemWidget(item);
            if (widget) {
                listWidget_->removeItemWidget(item);
                widget->deleteLater();
            }
        }
        renderedIndices_.remove(index);
    }
}

void ClipboardList::getVisibleRange(int& firstVisible, int& lastVisible) const
{
    firstVisible = -1;
    lastVisible = -1;

    if (listWidget_->count() == 0) {
        return;
    }

    QRect viewportRect = listWidget_->viewport()->rect();
    
    // 查找第一个可见项
    for (int i = 0; i < listWidget_->count(); ++i) {
        auto* item = listWidget_->item(i);
        QRect itemRect = listWidget_->visualItemRect(item);
        if (itemRect.intersects(viewportRect)) {
            firstVisible = i;
            break;
        }
    }

    // 查找最后一个可见项
    for (int i = listWidget_->count() - 1; i >= 0; --i) {
        auto* item = listWidget_->item(i);
        QRect itemRect = listWidget_->visualItemRect(item);
        if (itemRect.intersects(viewportRect)) {
            lastVisible = i;
            break;
        }
    }
}

void ClipboardList::updateVisibleWidgets()
{
    int firstVisible, lastVisible;
    getVisibleRange(firstVisible, lastVisible);

    if (firstVisible < 0 || lastVisible < 0) {
        return;
    }

    // 扩展渲染范围（预渲染缓冲区）
    int renderStart = qMax(0, firstVisible - VISIBLE_BUFFER);
    int renderEnd = qMin(listWidget_->count() - 1, lastVisible + VISIBLE_BUFFER);

    // 为可见范围内的项创建 widget
    for (int i = renderStart; i <= renderEnd; ++i) {
        ensureWidgetForIndex(i);
    }

    // 回收不可见的 widget
    recycleInvisibleWidgets();
}

void ClipboardList::onScrollingStopped()
{
    // 滚动停止后，确保所有可见项都有 widget
    updateVisibleWidgets();
}

void ClipboardList::filterByKeyword(const QString& keyword)
{
    currentKeyword_ = keyword.trimmed();

    // 清空当前列表
    listWidget_->clear();
    selectedRecordId_ = -1;
    renderedIndices_.clear();
    allRecords_.clear();

    if (currentKeyword_.isEmpty()) {
        // 关键词为空，退出过滤模式，重新加载
        isFiltering_ = false;
        loadedCount_ = 0;
        hasMoreRecords_ = true;
        loadMoreRecords();
        return;
    }

    // 进入过滤模式
    isFiltering_ = true;

    // 使用 FTS 搜索
    auto results = ClipboardStore::instance().searchText(
        currentKeyword_.toStdString(), 
        1000  // 搜索时返回更多结果
    );

    // 添加搜索结果到列表
    for (const auto& record : results) {
        allRecords_.push_back(record);
        addRecordToList(record);
    }

    // 更新可见区域的 widget
    updateVisibleWidgets();

    // 更新空列表提示
    if (results.empty()) {
        showEmptyHint(true, "无匹配结果");
    } else {
        showEmptyHint(false);
    }

    emit loadCompleted(static_cast<int>(results.size()));
}

void ClipboardList::refresh()
{
    if (isFiltering_ && !currentKeyword_.isEmpty()) {
        // 过滤模式下，重新执行搜索
        filterByKeyword(currentKeyword_);
    } else {
        // 正常模式下，重新加载
        loadRecords();
    }
}

void ClipboardList::clear()
{
    listWidget_->clear();
    allRecords_.clear();
    loadedCount_ = 0;
    hasMoreRecords_ = true;
    selectedRecordId_ = -1;
    renderedIndices_.clear();
    showEmptyHint(false);
}

int ClipboardList::recordCount() const
{
    return listWidget_->count();
}

int64_t ClipboardList::selectedRecordId() const
{
    return selectedRecordId_;
}

void ClipboardList::selectRecord(int64_t recordId)
{
    // 先取消之前的选中状态
    for (int i = 0; i < listWidget_->count(); ++i) {
        auto* item = listWidget_->item(i);
        auto* widget = qobject_cast<ClipboardItemWidget*>(
            listWidget_->itemWidget(item)
        );
        if (widget) {
            if (widget->recordId() == recordId) {
                widget->setSelected(true);
                selectedRecordId_ = recordId;
                listWidget_->scrollToItem(item);
            } else if (widget->recordId() == selectedRecordId_) {
                widget->setSelected(false);
            }
        }
    }
    
    selectedRecordId_ = recordId;
}

void ClipboardList::updateTimestamps()
{
    // 只更新已渲染的 widget
    for (int index : renderedIndices_) {
        auto* item = listWidget_->item(index);
        if (item) {
            auto* widget = qobject_cast<ClipboardItemWidget*>(
                listWidget_->itemWidget(item)
            );
            if (widget) {
                widget->updateTimestamp();
            }
        }
    }
}

void ClipboardList::onScrollValueChanged(int value)
{
    // 重启滚动停止检测定时器
    scrollStopTimer_->start(SCROLL_STOP_DELAY_MS);
    lastScrollValue_ = value;

    // 检查是否滚动到底部附近，触发懒加载
    QScrollBar* scrollBar = listWidget_->verticalScrollBar();
    int maxValue = scrollBar->maximum();

    if (maxValue - value < SCROLL_THRESHOLD) {
        loadMoreRecords();
    }

    // 实时更新可见区域的 widget
    updateVisibleWidgets();
}

void ClipboardList::onItemClicked(int64_t recordId)
{
    // 更新选中状态
    selectRecord(recordId);

    // 发射选中信号
    emit itemSelected(recordId);
}

void ClipboardList::updateEmptyHint()
{
    if (listWidget_->count() == 0) {
        if (isFiltering_) {
            showEmptyHint(true, "无匹配结果");
        } else {
            showEmptyHint(true, "暂无剪贴板历史");
            emit listEmpty();
        }
    } else {
        showEmptyHint(false);
    }
}

void ClipboardList::showEmptyHint(bool show, const QString& message)
{
    if (show) {
        emptyHintLabel_->setText(message);
        emptyHintLabel_->show();
        listWidget_->hide();
    } else {
        emptyHintLabel_->hide();
        listWidget_->show();
    }
}

} // namespace suyan
