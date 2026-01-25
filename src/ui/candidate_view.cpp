/**
 * CandidateView 实现
 */

#include "candidate_view.h"
#include "../core/input_engine.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>
#include <algorithm>

namespace suyan {

// ========== 构造/析构 ==========

CandidateView::CandidateView(QWidget* parent)
    : QWidget(parent)
{
    // 设置默认主题
    theme_ = Theme::defaultLight();
    
    // 启用鼠标追踪（用于悬停效果）
    setMouseTracking(true);
    
    // 设置背景透明（由 paintEvent 绘制背景）
    setAttribute(Qt::WA_TranslucentBackground);
}

CandidateView::~CandidateView() = default;

// ========== 数据更新 ==========

void CandidateView::setCandidates(const std::vector<CandidateItem>& candidates) {
    candidates_ = candidates;
    layoutDirty_ = true;
    updateGeometry();
    update();
}

void CandidateView::setPreedit(const QString& preedit) {
    if (preedit_ != preedit) {
        preedit_ = preedit;
        layoutDirty_ = true;
        updateGeometry();
        update();
    }
}

void CandidateView::setHighlightedIndex(int index) {
    if (highlightedIndex_ != index) {
        highlightedIndex_ = index;
        update();
    }
}

void CandidateView::updateFromState(const InputState& state) {
    // 更新 preedit
    preedit_ = QString::fromStdString(state.preedit);
    
    // 更新候选词（从 InputCandidate 转换为 CandidateItem）
    candidates_.clear();
    candidates_.reserve(state.candidates.size());
    for (const auto& c : state.candidates) {
        CandidateItem item;
        item.text = c.text;
        item.comment = c.comment;
        item.index = c.index;
        candidates_.push_back(item);
    }
    
    // 更新展开模式状态
    isExpanded_ = state.isExpanded;
    expandedRows_ = state.expandedRows;
    currentRow_ = state.currentRow;
    currentCol_ = state.currentCol;
    pageSize_ = state.pageSize > 0 ? state.pageSize : 9;
    
    // 更新高亮索引
    highlightedIndex_ = state.highlightedIndex;
    
    layoutDirty_ = true;
    updateGeometry();
    update();
}

// ========== 布局设置 ==========

void CandidateView::setLayoutType(LayoutType type) {
    if (layoutType_ != type) {
        layoutType_ = type;
        layoutDirty_ = true;
        updateGeometry();
        update();
    }
}

// ========== 主题设置 ==========

void CandidateView::setTheme(const Theme& theme) {
    theme_ = theme;
    layoutDirty_ = true;
    updateGeometry();
    update();
}

// ========== 多行展开模式 ==========

void CandidateView::setExpandedMode(bool expanded, int rows, int currentRow, int currentCol) {
    isExpanded_ = expanded;
    expandedRows_ = rows;
    currentRow_ = currentRow;
    currentCol_ = currentCol;
    layoutDirty_ = true;
    updateGeometry();
    update();
}

// ========== 尺寸计算 ==========

QSize CandidateView::calculateMinimumSize() const {
    if (!layoutDirty_ && cachedSize_.isValid()) {
        return cachedSize_;
    }
    
    int width = 0;
    int height = 0;
    
    // Preedit 高度
    int preeditHeight = calculatePreeditHeight();
    if (preeditHeight > 0) {
        height += preeditHeight;
    }
    
    // 候选词区域
    if (!candidates_.empty()) {
        int candidateHeight = calculateCandidateHeight();
        
        if (isExpanded_) {
            // 展开模式：根据布局类型计算尺寸
            int groupCount = std::min(expandedRows_, 
                                    (static_cast<int>(candidates_.size()) + pageSize_ - 1) / pageSize_);
            
            if (layoutType_ == LayoutType::Vertical) {
                // 竖排展开：多列布局
                // 每列有 pageSize_ 个候选词，共 groupCount 列
                
                // 计算每列的最大宽度
                std::vector<int> colWidths(groupCount, 0);
                for (int col = 0; col < groupCount; ++col) {
                    int startIdx = col * pageSize_;
                    int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
                    for (int i = startIdx; i < endIdx; ++i) {
                        int w = calculateCandidateWidth(candidates_[i]);
                        if (w > colWidths[col]) {
                            colWidths[col] = w;
                        }
                    }
                }
                
                // 总宽度 = 所有列宽度之和 + 列间距
                int totalWidth = 0;
                for (int col = 0; col < groupCount; ++col) {
                    totalWidth += colWidths[col];
                    if (col < groupCount - 1) {
                        totalWidth += theme_.candidateSpacing;
                    }
                }
                width = std::max(width, totalWidth);
                
                // 高度 = pageSize_ 个候选词的高度
                int maxRowsInAnyCol = 0;
                for (int col = 0; col < groupCount; ++col) {
                    int startIdx = col * pageSize_;
                    int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
                    int rowsInCol = endIdx - startIdx;
                    maxRowsInAnyCol = std::max(maxRowsInAnyCol, rowsInCol);
                }
                height += maxRowsInAnyCol * candidateHeight;
                height += (maxRowsInAnyCol - 1) * theme_.candidateSpacing;
            } else {
                // 横排展开：多行布局（原有逻辑）
                // 计算每行的最大宽度
                int maxRowWidth = 0;
                for (int row = 0; row < groupCount; ++row) {
                    int rowWidth = 0;
                    int startIdx = row * pageSize_;
                    int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
                    
                    for (int i = startIdx; i < endIdx; ++i) {
                        rowWidth += calculateCandidateWidth(candidates_[i]);
                        rowWidth += theme_.candidateSpacing;
                    }
                    if (endIdx > startIdx) {
                        rowWidth -= theme_.candidateSpacing;
                    }
                    maxRowWidth = std::max(maxRowWidth, rowWidth);
                }
                
                width = std::max(width, maxRowWidth);
                height += groupCount * candidateHeight;
                height += (groupCount - 1) * theme_.candidateSpacing;
            }
        } else if (layoutType_ == LayoutType::Horizontal) {
            // 横排：计算总宽度
            int totalWidth = 0;
            for (const auto& candidate : candidates_) {
                totalWidth += calculateCandidateWidth(candidate);
                totalWidth += theme_.candidateSpacing;
            }
            if (!candidates_.empty()) {
                totalWidth -= theme_.candidateSpacing;  // 移除最后一个间距
            }
            width = std::max(width, totalWidth);
            height += candidateHeight;
        } else {
            // 竖排：计算最大宽度和总高度
            int maxWidth = 0;
            for (const auto& candidate : candidates_) {
                maxWidth = std::max(maxWidth, calculateCandidateWidth(candidate));
            }
            width = std::max(width, maxWidth);
            height += static_cast<int>(candidates_.size()) * candidateHeight;
            height += static_cast<int>(candidates_.size() - 1) * theme_.candidateSpacing;
        }
    }
    
    // 添加内边距
    width += theme_.padding * 2;
    height += theme_.padding * 2;
    
    // 确保最小尺寸
    width = std::max(width, 100);
    height = std::max(height, 30);
    
    cachedSize_ = QSize(width, height);
    layoutDirty_ = false;
    
    return cachedSize_;
}

QSize CandidateView::sizeHint() const {
    return calculateMinimumSize();
}

QSize CandidateView::minimumSizeHint() const {
    return calculateMinimumSize();
}

// ========== 绘制事件 ==========

void CandidateView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // 绘制背景
    drawBackground(painter);
    
    // 计算起始 Y 偏移
    int yOffset = theme_.padding;
    
    // 绘制 preedit
    drawPreedit(painter, yOffset);
    
    // 绘制候选词
    if (isExpanded_) {
        // 展开模式：根据布局类型选择绘制方法
        if (layoutType_ == LayoutType::Vertical) {
            drawCandidatesExpandedVertical(painter, yOffset);
        } else {
            drawCandidatesExpanded(painter, yOffset);
        }
    } else if (layoutType_ == LayoutType::Horizontal) {
        drawCandidatesHorizontal(painter, yOffset);
    } else {
        drawCandidatesVertical(painter, yOffset);
    }
}

void CandidateView::drawBackground(QPainter& painter) {
    // 创建圆角矩形路径
    QPainterPath path;
    path.addRoundedRect(rect(), theme_.borderRadius, theme_.borderRadius);
    
    // 绘制背景
    QColor bgColor = theme_.backgroundColor;
    bgColor.setAlpha(theme_.backgroundOpacity * 255 / 100);
    painter.fillPath(path, bgColor);
    
    // 绘制边框
    if (theme_.borderWidth > 0) {
        painter.setPen(QPen(theme_.borderColor, theme_.borderWidth));
        painter.drawPath(path);
    }
}

void CandidateView::drawPreedit(QPainter& painter, int& yOffset) {
    if (preedit_.isEmpty() || !showPreedit_) {
        return;
    }
    
    QFont font = getPreeditFont();
    painter.setFont(font);
    painter.setPen(theme_.preeditColor);
    
    QFontMetrics fm(font);
    int textHeight = fm.height();
    
    QRect textRect(theme_.padding, yOffset, 
                   width() - theme_.padding * 2, textHeight);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, preedit_);
    
    yOffset += textHeight + theme_.candidateSpacing;
}

void CandidateView::drawCandidatesHorizontal(QPainter& painter, int yOffset) {
    if (candidates_.empty()) {
        return;
    }
    
    candidateRects_.clear();
    candidateRects_.reserve(candidates_.size());
    
    int xOffset = theme_.padding;
    int candidateHeight = calculateCandidateHeight();
    
    for (size_t i = 0; i < candidates_.size(); ++i) {
        const auto& candidate = candidates_[i];
        int candidateWidth = calculateCandidateWidth(candidate);
        
        QRect rect(xOffset, yOffset, candidateWidth, candidateHeight);
        candidateRects_.push_back(rect);
        
        bool isHighlighted = (static_cast<int>(i) == highlightedIndex_);
        bool isHovered = (static_cast<int>(i) == hoveredIndex_);
        
        drawSingleCandidate(painter, candidate, rect, isHighlighted, isHovered);
        
        xOffset += candidateWidth + theme_.candidateSpacing;
    }
}

void CandidateView::drawCandidatesVertical(QPainter& painter, int yOffset) {
    if (candidates_.empty()) {
        return;
    }
    
    candidateRects_.clear();
    candidateRects_.reserve(candidates_.size());
    
    int candidateHeight = calculateCandidateHeight();
    int contentWidth = width() - theme_.padding * 2;
    
    for (size_t i = 0; i < candidates_.size(); ++i) {
        const auto& candidate = candidates_[i];
        
        QRect rect(theme_.padding, yOffset, contentWidth, candidateHeight);
        candidateRects_.push_back(rect);
        
        bool isHighlighted = (static_cast<int>(i) == highlightedIndex_);
        bool isHovered = (static_cast<int>(i) == hoveredIndex_);
        
        drawSingleCandidate(painter, candidate, rect, isHighlighted, isHovered);
        
        yOffset += candidateHeight + theme_.candidateSpacing;
    }
}

void CandidateView::drawCandidatesExpanded(QPainter& painter, int yOffset) {
    if (candidates_.empty()) {
        return;
    }
    
    candidateRects_.clear();
    candidateRects_.reserve(candidates_.size());
    
    int candidateHeight = calculateCandidateHeight();
    int rowCount = std::min(expandedRows_, 
                            (static_cast<int>(candidates_.size()) + pageSize_ - 1) / pageSize_);
    
    // 计算每列的最大宽度，使列对齐
    std::vector<int> colWidths(pageSize_, 0);
    for (int row = 0; row < rowCount; ++row) {
        int startIdx = row * pageSize_;
        int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
        for (int i = startIdx; i < endIdx; ++i) {
            int col = i - startIdx;
            int w = calculateCandidateWidth(candidates_[i]);
            if (w > colWidths[col]) {
                colWidths[col] = w;
            }
        }
    }
    
    for (int row = 0; row < rowCount; ++row) {
        int xOffset = theme_.padding;
        int startIdx = row * pageSize_;
        int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
        
        // 当前行高亮显示数字，其他行数字灰色
        bool isCurrentGroup = (row == currentRow_);
        
        for (int i = startIdx; i < endIdx; ++i) {
            const auto& candidate = candidates_[i];
            int indexInGroup = i - startIdx;
            // 使用该列的最大宽度
            int candidateWidth = colWidths[indexInGroup];
            
            QRect rect(xOffset, yOffset, candidateWidth, candidateHeight);
            candidateRects_.push_back(rect);
            
            bool isHighlighted = (row == currentRow_ && indexInGroup == currentCol_);
            bool isHovered = (static_cast<int>(i) == hoveredIndex_);
            
            drawSingleCandidateExpanded(painter, candidate, rect, isHighlighted, isHovered, isCurrentGroup, indexInGroup);
            
            xOffset += candidateWidth + theme_.candidateSpacing;
        }
        
        yOffset += candidateHeight + theme_.candidateSpacing;
    }
}

void CandidateView::drawCandidatesExpandedVertical(QPainter& painter, int yOffset) {
    // 竖排展开模式：多列布局
    // 每列有 pageSize_ 个候选词，共 expandedRows_ 列
    // currentRow_ 表示当前选中的列（组）
    // currentCol_ 表示当前列内的索引
    
    if (candidates_.empty()) {
        return;
    }
    
    candidateRects_.clear();
    candidateRects_.reserve(candidates_.size());
    
    int candidateHeight = calculateCandidateHeight();
    int colCount = std::min(expandedRows_, 
                            (static_cast<int>(candidates_.size()) + pageSize_ - 1) / pageSize_);
    
    // 计算每列的最大宽度
    std::vector<int> colWidths(colCount, 0);
    for (int col = 0; col < colCount; ++col) {
        int startIdx = col * pageSize_;
        int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
        for (int i = startIdx; i < endIdx; ++i) {
            int w = calculateCandidateWidth(candidates_[i]);
            if (w > colWidths[col]) {
                colWidths[col] = w;
            }
        }
    }
    
    // 计算每列的 X 起始位置
    std::vector<int> colXOffsets(colCount);
    int xOffset = theme_.padding;
    for (int col = 0; col < colCount; ++col) {
        colXOffsets[col] = xOffset;
        xOffset += colWidths[col] + theme_.candidateSpacing;
    }
    
    // 按列绘制候选词
    for (int col = 0; col < colCount; ++col) {
        int startIdx = col * pageSize_;
        int endIdx = std::min(startIdx + pageSize_, static_cast<int>(candidates_.size()));
        int currentYOffset = yOffset;
        
        // 当前列高亮显示数字，其他列数字灰色
        bool isCurrentGroup = (col == currentRow_);
        
        for (int i = startIdx; i < endIdx; ++i) {
            const auto& candidate = candidates_[i];
            int indexInGroup = i - startIdx;  // 列内索引
            
            QRect rect(colXOffsets[col], currentYOffset, colWidths[col], candidateHeight);
            candidateRects_.push_back(rect);
            
            // 高亮条件：当前列且当前列内索引匹配
            bool isHighlighted = (col == currentRow_ && indexInGroup == currentCol_);
            bool isHovered = (static_cast<int>(i) == hoveredIndex_);
            
            drawSingleCandidateExpanded(painter, candidate, rect, isHighlighted, isHovered, isCurrentGroup, indexInGroup);
            
            currentYOffset += candidateHeight + theme_.candidateSpacing;
        }
    }
}

void CandidateView::drawSingleCandidate(QPainter& painter, 
                                         const CandidateItem& candidate,
                                         const QRect& rect, 
                                         bool isHighlighted, 
                                         bool isHovered) {
    // 绘制高亮/悬停背景
    if (isHighlighted) {
        QPainterPath path;
        path.addRoundedRect(rect, 4, 4);
        painter.fillPath(path, theme_.highlightBackColor);
    } else if (isHovered) {
        QPainterPath path;
        path.addRoundedRect(rect, 4, 4);
        QColor hoverColor = theme_.highlightBackColor;
        hoverColor.setAlpha(50);
        painter.fillPath(path, hoverColor);
    }
    
    // 计算文本位置
    int xPos = rect.left() + 4;
    int yCenter = rect.center().y();
    
    // 绘制序号标签
    QFont labelFont = getLabelFont();
    painter.setFont(labelFont);
    QFontMetrics labelFm(labelFont);
    
    QString label = QString::number(candidate.index) + ".";
    painter.setPen(isHighlighted ? theme_.highlightTextColor : theme_.labelColor);
    
    int labelWidth = labelFm.horizontalAdvance(label);
    QRect labelRect(xPos, rect.top(), labelWidth, rect.height());
    painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    xPos += labelWidth + 4;
    
    // 绘制候选词文本
    QFont candidateFont = getCandidateFont();
    painter.setFont(candidateFont);
    QFontMetrics candidateFm(candidateFont);
    
    QString text = QString::fromStdString(candidate.text);
    painter.setPen(isHighlighted ? theme_.highlightTextColor : theme_.textColor);
    
    int textWidth = candidateFm.horizontalAdvance(text);
    QRect textRect(xPos, rect.top(), textWidth, rect.height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    xPos += textWidth;
    
    // 绘制注释（如果有且启用显示）
    if (showComment_ && !candidate.comment.empty()) {
        QFont commentFont = getCommentFont();
        painter.setFont(commentFont);
        
        QString comment = QString::fromStdString(candidate.comment);
        painter.setPen(isHighlighted ? theme_.highlightTextColor : theme_.commentColor);
        
        xPos += 4;
        QRect commentRect(xPos, rect.top(), rect.right() - xPos, rect.height());
        painter.drawText(commentRect, Qt::AlignLeft | Qt::AlignVCenter, comment);
    }
}

void CandidateView::drawSingleCandidateExpanded(QPainter& painter, 
                                                 const CandidateItem& candidate,
                                                 const QRect& rect, 
                                                 bool isHighlighted, 
                                                 bool isHovered,
                                                 bool isCurrentGroup,
                                                 int indexInGroup) {
    // 绘制高亮/悬停背景
    if (isHighlighted) {
        QPainterPath path;
        path.addRoundedRect(rect, 4, 4);
        painter.fillPath(path, theme_.highlightBackColor);
    } else if (isHovered) {
        QPainterPath path;
        path.addRoundedRect(rect, 4, 4);
        QColor hoverColor = theme_.highlightBackColor;
        hoverColor.setAlpha(50);
        painter.fillPath(path, hoverColor);
    }
    
    // 计算文本位置
    int xPos = rect.left() + 4;
    
    // 绘制序号标签（所有组都显示，但非当前组用更淡的颜色）
    QFont labelFont = getLabelFont();
    painter.setFont(labelFont);
    QFontMetrics labelFm(labelFont);
    
    // 使用组内索引+1作为序号（1-9），使用英文点号
    QString label = QString::number(indexInGroup + 1) + ".";
    
    if (isHighlighted) {
        painter.setPen(theme_.highlightTextColor);
    } else if (isCurrentGroup) {
        painter.setPen(theme_.labelColor);
    } else {
        // 非当前组，数字用更淡的颜色
        QColor dimColor = theme_.labelColor;
        dimColor.setAlpha(80);
        painter.setPen(dimColor);
    }
    
    int labelWidth = labelFm.horizontalAdvance(label);
    QRect labelRect(xPos, rect.top(), labelWidth, rect.height());
    painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    xPos += labelWidth + 4;
    
    // 绘制候选词文本
    QFont candidateFont = getCandidateFont();
    painter.setFont(candidateFont);
    QFontMetrics candidateFm(candidateFont);
    
    QString text = QString::fromStdString(candidate.text);
    painter.setPen(isHighlighted ? theme_.highlightTextColor : theme_.textColor);
    
    int textWidth = candidateFm.horizontalAdvance(text);
    QRect textRect(xPos, rect.top(), textWidth, rect.height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    xPos += textWidth;
    
    // 绘制注释（如果有且启用显示）
    if (showComment_ && !candidate.comment.empty()) {
        QFont commentFont = getCommentFont();
        painter.setFont(commentFont);
        
        QString comment = QString::fromStdString(candidate.comment);
        painter.setPen(isHighlighted ? theme_.highlightTextColor : theme_.commentColor);
        
        xPos += 4;
        QRect commentRect(xPos, rect.top(), rect.right() - xPos, rect.height());
        painter.drawText(commentRect, Qt::AlignLeft | Qt::AlignVCenter, comment);
    }
}

// ========== 鼠标事件 ==========

void CandidateView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int index = getCandidateAtPosition(event->pos());
        if (index >= 0) {
            emit candidateClicked(index);
        }
    }
    QWidget::mousePressEvent(event);
}

void CandidateView::mouseMoveEvent(QMouseEvent* event) {
    int index = getCandidateAtPosition(event->pos());
    if (hoveredIndex_ != index) {
        hoveredIndex_ = index;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CandidateView::leaveEvent(QEvent* event) {
    if (hoveredIndex_ != -1) {
        hoveredIndex_ = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

// ========== 布局计算 ==========

QRect CandidateView::calculateCandidateRect(int index) const {
    if (index >= 0 && index < static_cast<int>(candidateRects_.size())) {
        return candidateRects_[index];
    }
    return QRect();
}

int CandidateView::getCandidateAtPosition(const QPoint& pos) const {
    for (size_t i = 0; i < candidateRects_.size(); ++i) {
        if (candidateRects_[i].contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int CandidateView::calculatePreeditHeight() const {
    if (preedit_.isEmpty() || !showPreedit_) {
        return 0;
    }
    
    QFont font = getPreeditFont();
    QFontMetrics fm(font);
    return fm.height();
}

int CandidateView::calculateCandidateHeight() const {
    QFont font = getCandidateFont();
    QFontMetrics fm(font);
    return fm.height() + 8;  // 添加上下内边距
}

int CandidateView::calculateCandidateWidth(const CandidateItem& candidate) const {
    int width = 8;  // 左右内边距
    
    // 序号标签宽度
    QFont labelFont = getLabelFont();
    QFontMetrics labelFm(labelFont);
    QString label = QString::number(candidate.index) + ".";
    width += labelFm.horizontalAdvance(label) + 4;
    
    // 候选词文本宽度
    QFont candidateFont = getCandidateFont();
    QFontMetrics candidateFm(candidateFont);
    QString text = QString::fromStdString(candidate.text);
    width += candidateFm.horizontalAdvance(text);
    
    // 注释宽度（如果有且启用显示）
    if (showComment_ && !candidate.comment.empty()) {
        QFont commentFont = getCommentFont();
        QFontMetrics commentFm(commentFont);
        QString comment = QString::fromStdString(candidate.comment);
        width += 4 + commentFm.horizontalAdvance(comment);
    }
    
    return width;
}

int CandidateView::calculateCandidateWidthExpanded(const CandidateItem& candidate, bool showLabel) const {
    int width = 8;  // 左右内边距
    
    // 序号标签宽度（仅当前行显示）
    if (showLabel) {
        QFont labelFont = getLabelFont();
        QFontMetrics labelFm(labelFont);
        QString label = QString::number((candidate.index - 1) % pageSize_ + 1) + ".";
        width += labelFm.horizontalAdvance(label) + 4;
    }
    
    // 候选词文本宽度
    QFont candidateFont = getCandidateFont();
    QFontMetrics candidateFm(candidateFont);
    QString text = QString::fromStdString(candidate.text);
    width += candidateFm.horizontalAdvance(text);
    
    // 注释宽度（如果有且启用显示）
    if (showComment_ && !candidate.comment.empty()) {
        QFont commentFont = getCommentFont();
        QFontMetrics commentFm(commentFont);
        QString comment = QString::fromStdString(candidate.comment);
        width += 4 + commentFm.horizontalAdvance(comment);
    }
    
    return width;
}

// ========== 字体获取 ==========

QFont CandidateView::getPreeditFont() const {
    QFont font(theme_.fontFamily);
    font.setPixelSize(theme_.fontSize - 2);  // preedit 稍小
    return font;
}

QFont CandidateView::getCandidateFont() const {
    QFont font(theme_.fontFamily);
    font.setPixelSize(theme_.fontSize);
    return font;
}

QFont CandidateView::getLabelFont() const {
    QFont font(theme_.fontFamily);
    font.setPixelSize(theme_.fontSize - 2);  // 序号稍小
    return font;
}

QFont CandidateView::getCommentFont() const {
    QFont font(theme_.fontFamily);
    font.setPixelSize(theme_.fontSize - 4);  // 注释更小
    return font;
}

} // namespace suyan
