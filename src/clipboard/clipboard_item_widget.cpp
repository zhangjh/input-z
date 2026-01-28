/**
 * ClipboardItemWidget 实现
 *
 * Requirements: 5.3-5.6
 */

#include "clipboard_item_widget.h"

#include <QDateTime>
#include <QFileInfo>
#include <QPainter>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QPalette>

namespace suyan {

ClipboardItemWidget::ClipboardItemWidget(const ClipboardRecord& record, QWidget* parent)
    : QWidget(parent)
    , recordId_(record.id)
    , contentType_(record.type)
    , lastUsedAt_(record.lastUsedAt)
    , sourceApp_(QString::fromStdString(record.sourceApp))
    , content_(QString::fromStdString(record.content))
    , thumbnailPath_(QString::fromStdString(record.thumbnailPath))
{
    setupUI();
    setFixedHeight(WIDGET_HEIGHT);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void ClipboardItemWidget::setupUI()
{
    // 主布局：水平布局
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(12);

    // 左侧：缩略图或图标区域（仅图片类型显示）
    if (contentType_ == ClipboardContentType::Image) {
        thumbnailLabel_ = new QLabel(this);
        thumbnailLabel_->setFixedSize(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);
        thumbnailLabel_->setAlignment(Qt::AlignCenter);
        thumbnailLabel_->setStyleSheet(
            "QLabel { background-color: #f0f0f0; border-radius: 4px; }"
        );

        // 加载缩略图
        if (!thumbnailPath_.isEmpty() && QFileInfo::exists(thumbnailPath_)) {
            QPixmap thumbnail(thumbnailPath_);
            if (!thumbnail.isNull()) {
                thumbnail = thumbnail.scaled(
                    THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                );
                thumbnailLabel_->setPixmap(thumbnail);
            } else {
                thumbnailLabel_->setText("[图片]");
            }
        } else {
            thumbnailLabel_->setText("[图片]");
        }

        mainLayout->addWidget(thumbnailLabel_);
    }

    // 中间：内容区域（垂直布局）
    auto* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    // 内容预览
    contentLabel_ = new QLabel(this);
    contentLabel_->setWordWrap(false);
    contentLabel_->setTextFormat(Qt::PlainText);

    if (contentType_ == ClipboardContentType::Text) {
        // 文本：截断显示前 100 字符
        QString preview = truncateText(content_, MAX_TEXT_PREVIEW_LENGTH);
        // 替换换行符为空格，保持单行显示
        preview.replace('\n', ' ');
        preview.replace('\r', ' ');
        preview.replace('\t', ' ');
        // 压缩连续空格
        preview = preview.simplified();
        contentLabel_->setText(preview);
    } else if (contentType_ == ClipboardContentType::Image) {
        // 图片：显示格式和尺寸信息
        contentLabel_->setText(QString("[图片]"));
    } else {
        contentLabel_->setText("[未知内容]");
    }

    contentLabel_->setStyleSheet(
        "QLabel { color: #333333; font-size: 13px; }"
    );
    contentLayout->addWidget(contentLabel_);

    // 底部信息行：时间戳
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);

    // 时间戳
    timestampLabel_ = new QLabel(this);
    timestampLabel_->setText(formatRelativeTime(lastUsedAt_));
    timestampLabel_->setStyleSheet(
        "QLabel { color: #999999; font-size: 11px; }"
    );
    infoLayout->addWidget(timestampLabel_);

    infoLayout->addStretch();
    contentLayout->addLayout(infoLayout);

    mainLayout->addLayout(contentLayout, 1);
}

QString ClipboardItemWidget::formatRelativeTime(int64_t timestampMs)
{
    QDateTime recordTime = QDateTime::fromMSecsSinceEpoch(timestampMs);
    QDateTime now = QDateTime::currentDateTime();
    qint64 diffSecs = recordTime.secsTo(now);

    if (diffSecs < 0) {
        return "刚刚";
    }

    if (diffSecs < 60) {
        return "刚刚";
    }

    qint64 diffMins = diffSecs / 60;
    if (diffMins < 60) {
        return QString("%1分钟前").arg(diffMins);
    }

    qint64 diffHours = diffMins / 60;
    if (diffHours < 24) {
        return QString("%1小时前").arg(diffHours);
    }

    qint64 diffDays = diffHours / 24;
    if (diffDays < 7) {
        return QString("%1天前").arg(diffDays);
    }

    if (diffDays < 30) {
        qint64 weeks = diffDays / 7;
        return QString("%1周前").arg(weeks);
    }

    if (diffDays < 365) {
        qint64 months = diffDays / 30;
        return QString("%1个月前").arg(months);
    }

    qint64 years = diffDays / 365;
    return QString("%1年前").arg(years);
}

QString ClipboardItemWidget::truncateText(const QString& text, int maxLength)
{
    if (text.length() <= maxLength) {
        return text;
    }
    return text.left(maxLength) + "...";
}

QString ClipboardItemWidget::getAppDisplayName(const QString& bundleId)
{
    // 常见应用的友好名称映射
    static const QMap<QString, QString> appNames = {
        {"com.apple.Safari", "Safari"},
        {"com.apple.finder", "访达"},
        {"com.apple.Terminal", "终端"},
        {"com.apple.TextEdit", "文本编辑"},
        {"com.apple.Notes", "备忘录"},
        {"com.apple.mail", "邮件"},
        {"com.apple.Preview", "预览"},
        {"com.google.Chrome", "Chrome"},
        {"org.mozilla.firefox", "Firefox"},
        {"com.microsoft.VSCode", "VS Code"},
        {"com.sublimetext.4", "Sublime Text"},
        {"com.jetbrains.intellij", "IntelliJ IDEA"},
        {"com.tencent.xinWeChat", "微信"},
        {"com.tencent.qq", "QQ"},
        {"com.apple.dt.Xcode", "Xcode"},
        {"com.amazon.Kiro", "Kiro"},
    };

    if (appNames.contains(bundleId)) {
        return appNames[bundleId];
    }

    // 尝试从 Bundle ID 提取应用名称
    // 例如 "com.example.MyApp" -> "MyApp"
    QStringList parts = bundleId.split('.');
    if (!parts.isEmpty()) {
        return parts.last();
    }

    return bundleId;
}

void ClipboardItemWidget::setSelected(bool selected)
{
    if (selected_ != selected) {
        selected_ = selected;
        updateBackgroundStyle();
        update();
    }
}

void ClipboardItemWidget::updateTimestamp()
{
    if (timestampLabel_) {
        timestampLabel_->setText(formatRelativeTime(lastUsedAt_));
    }
}

void ClipboardItemWidget::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    hovered_ = true;
    updateBackgroundStyle();
    update();
}

void ClipboardItemWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    hovered_ = false;
    updateBackgroundStyle();
    update();
}

void ClipboardItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(recordId_);
    }
    QWidget::mousePressEvent(event);
}

void ClipboardItemWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor;
    if (selected_) {
        // 选中状态：蓝色背景
        bgColor = QColor(0, 122, 255, 40);  // 半透明蓝色
    } else if (hovered_) {
        // 悬停状态：浅灰色背景
        bgColor = QColor(0, 0, 0, 20);  // 半透明黑色
    } else {
        // 默认状态：透明
        bgColor = Qt::transparent;
    }

    if (bgColor != Qt::transparent) {
        painter.fillRect(rect(), bgColor);
    }

    // 绘制底部分隔线
    painter.setPen(QColor(230, 230, 230));
    painter.drawLine(12, height() - 1, width() - 12, height() - 1);
}

void ClipboardItemWidget::updateBackgroundStyle()
{
    // 背景通过 paintEvent 绘制，这里可以添加其他样式更新
}

} // namespace suyan
