/**
 * ClipboardItemWidget - 剪贴板列表项组件
 *
 * 用于在剪贴板历史列表中渲染单条记录。
 * 显示时间戳、内容预览、来源应用信息。
 * 支持鼠标悬停高亮和点击选中效果。
 *
 * Requirements: 5.3-5.6
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_ITEM_WIDGET_H
#define SUYAN_CLIPBOARD_CLIPBOARD_ITEM_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QString>
#include <cstdint>

#include "clipboard_store.h"

namespace suyan {

/**
 * ClipboardItemWidget - 剪贴板列表项组件
 *
 * 继承 QWidget，实现单条剪贴板记录的渲染。
 */
class ClipboardItemWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * 构造函数
     *
     * @param record 剪贴板记录
     * @param parent 父组件
     */
    explicit ClipboardItemWidget(const ClipboardRecord& record, QWidget* parent = nullptr);
    ~ClipboardItemWidget() override = default;

    /**
     * 获取记录 ID
     */
    int64_t recordId() const { return recordId_; }

    /**
     * 获取记录类型
     */
    ClipboardContentType contentType() const { return contentType_; }

    /**
     * 设置选中状态
     */
    void setSelected(bool selected);

    /**
     * 获取选中状态
     */
    bool isSelected() const { return selected_; }

    /**
     * 更新时间戳显示（用于定时刷新相对时间）
     */
    void updateTimestamp();

signals:
    /**
     * 点击信号
     */
    void clicked(int64_t recordId);

protected:
    /**
     * 鼠标进入事件 - 高亮效果
     */
    void enterEvent(QEnterEvent* event) override;

    /**
     * 鼠标离开事件 - 取消高亮
     */
    void leaveEvent(QEvent* event) override;

    /**
     * 鼠标按下事件 - 触发点击
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * 绘制事件 - 自定义背景
     */
    void paintEvent(QPaintEvent* event) override;

private:
    /**
     * 初始化 UI
     */
    void setupUI();

    /**
     * 格式化相对时间
     *
     * @param timestampMs Unix 毫秒时间戳
     * @return 相对时间字符串（如"5分钟前"）
     */
    static QString formatRelativeTime(int64_t timestampMs);

    /**
     * 截断文本预览
     *
     * @param text 原始文本
     * @param maxLength 最大长度
     * @return 截断后的文本
     */
    static QString truncateText(const QString& text, int maxLength = 100);

    /**
     * 获取应用显示名称
     *
     * @param bundleId 应用 Bundle ID
     * @return 应用名称
     */
    static QString getAppDisplayName(const QString& bundleId);

    /**
     * 更新背景样式
     */
    void updateBackgroundStyle();

    // 记录数据
    int64_t recordId_;
    ClipboardContentType contentType_;
    int64_t lastUsedAt_;
    QString sourceApp_;
    QString content_;
    QString thumbnailPath_;

    // UI 组件
    QLabel* timestampLabel_ = nullptr;
    QLabel* contentLabel_ = nullptr;
    QLabel* thumbnailLabel_ = nullptr;

    // 状态
    bool hovered_ = false;
    bool selected_ = false;

    // 样式常量
    static constexpr int THUMBNAIL_WIDTH = 60;
    static constexpr int THUMBNAIL_HEIGHT = 40;
    static constexpr int MAX_TEXT_PREVIEW_LENGTH = 100;
    static constexpr int WIDGET_HEIGHT = 60;
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_ITEM_WIDGET_H
