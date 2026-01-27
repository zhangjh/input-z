/**
 * CandidateView - 候选词视图
 *
 * 负责渲染候选词列表，支持横排/竖排布局、高亮显示、主题样式。
 * 作为 CandidateWindow 的核心渲染组件。
 */

#ifndef SUYAN_UI_CANDIDATE_VIEW_H
#define SUYAN_UI_CANDIDATE_VIEW_H

#include <QWidget>
#include <QString>
#include <QFont>
#include <vector>
#include <string>
#include "theme_manager.h"
#include "layout_manager.h"

namespace suyan {

// 前向声明 InputState（避免包含 input_engine.h 导致的宏冲突）
struct InputState;

/**
 * 候选词结构（UI 层使用，与 InputCandidate 兼容）
 */
struct CandidateItem {
    std::string text;       // 候选词文本
    std::string comment;    // 注释（如拼音）
    int index;              // 序号 (1-based，用于显示)
};

/**
 * CandidateView - 候选词视图类
 *
 * 职责：
 * 1. 渲染候选词列表（横排/竖排）
 * 2. 显示 preedit（输入中的拼音）
 * 3. 高亮显示当前选中的候选词
 * 4. 显示序号标签（1-9）
 * 5. 应用主题样式
 */
class CandidateView : public QWidget {
    Q_OBJECT

public:
    explicit CandidateView(QWidget* parent = nullptr);
    ~CandidateView() override;

    // ========== 数据更新 ==========

    /**
     * 更新候选词列表
     *
     * @param candidates 候选词列表
     */
    void setCandidates(const std::vector<CandidateItem>& candidates);

    /**
     * 设置 preedit 文本
     *
     * @param preedit 当前输入的拼音
     */
    void setPreedit(const QString& preedit);

    /**
     * 设置高亮索引
     *
     * @param index 高亮的候选词索引 (0-based)，-1 表示无高亮
     */
    void setHighlightedIndex(int index);

    /**
     * 从 InputState 更新所有数据
     * 注意：需要在 .cpp 中包含 input_engine.h
     *
     * @param state 输入状态
     */
    void updateFromState(const InputState& state);

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
    LayoutType getLayoutType() const { return layoutType_; }

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
    const Theme& getTheme() const { return theme_; }

    // ========== 显示选项 ==========

    /**
     * 设置是否显示候选词注释（拼音）
     *
     * @param show 是否显示
     */
    void setShowComment(bool show) { showComment_ = show; layoutDirty_ = true; update(); }

    /**
     * 获取是否显示候选词注释
     */
    bool getShowComment() const { return showComment_; }

    /**
     * 设置是否显示 preedit
     *
     * @param show 是否显示
     */
    void setShowPreedit(bool show) { showPreedit_ = show; layoutDirty_ = true; update(); }

    /**
     * 获取是否显示 preedit
     */
    bool getShowPreedit() const { return showPreedit_; }

    // ========== 多行展开模式 ==========

    /**
     * 设置展开模式
     *
     * @param expanded 是否展开
     * @param rows 展开的行数（1-5）
     * @param currentRow 当前选中的行
     * @param currentCol 当前选中的列
     */
    void setExpandedMode(bool expanded, int rows, int currentRow, int currentCol);

    /**
     * 获取是否处于展开模式
     */
    bool isExpanded() const { return isExpanded_; }

    // ========== 尺寸计算 ==========

    /**
     * 计算所需的最小尺寸
     */
    QSize calculateMinimumSize() const;

    /**
     * 重写 sizeHint
     */
    QSize sizeHint() const override;

    /**
     * 重写 minimumSizeHint
     */
    QSize minimumSizeHint() const override;

signals:
    /**
     * 候选词被点击
     *
     * @param index 候选词索引 (0-based)
     */
    void candidateClicked(int index);

protected:
    /**
     * 重写绘制事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * 重写鼠标点击事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * 重写鼠标移动事件（用于悬停效果）
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * 重写鼠标离开事件
     */
    void leaveEvent(QEvent* event) override;

private:
    // 绘制方法
    void drawBackground(QPainter& painter);
    void drawPreedit(QPainter& painter, int& yOffset);
    void drawCandidatesHorizontal(QPainter& painter, int yOffset);
    void drawCandidatesVertical(QPainter& painter, int yOffset);
    void drawCandidatesExpanded(QPainter& painter, int yOffset);  // 横排展开模式（多行）
    void drawCandidatesExpandedVertical(QPainter& painter, int yOffset);  // 竖排展开模式（多列）
    void drawSingleCandidate(QPainter& painter, const CandidateItem& candidate,
                             const QRect& rect, bool isHighlighted, bool isHovered);
    void drawSingleCandidateExpanded(QPainter& painter, const CandidateItem& candidate,
                                      const QRect& rect, bool isHighlighted, bool isHovered,
                                      bool isCurrentGroup, int indexInGroup);

    // 布局计算
    QRect calculateCandidateRect(int index) const;
    int getCandidateAtPosition(const QPoint& pos) const;
    int calculatePreeditHeight() const;
    int calculateCandidateHeight() const;
    int calculateCandidateWidth(const CandidateItem& candidate) const;
    int calculateCandidateWidthExpanded(const CandidateItem& candidate, bool showLabel) const;

    // 字体获取
    QFont getPreeditFont() const;
    QFont getCandidateFont() const;
    QFont getLabelFont() const;
    QFont getCommentFont() const;

    // 数据
    std::vector<CandidateItem> candidates_;
    QString preedit_;
    int highlightedIndex_ = -1;
    int hoveredIndex_ = -1;

    // 配置
    LayoutType layoutType_ = LayoutType::Horizontal;
    Theme theme_;

    // 显示选项
    bool showComment_ = false;   // 默认不显示候选词注释（拼音）
    bool showPreedit_ = true;    // 默认显示 preedit（Windows TSF 需要在候选框中显示）

    // 多行展开模式
    bool isExpanded_ = false;    // 是否展开多行
    int expandedRows_ = 1;       // 展开的行数
    int currentRow_ = 0;         // 当前选中的行
    int currentCol_ = 0;         // 当前选中的列
    int pageSize_ = 9;           // 每行候选词数量

    // 缓存的布局信息
    mutable std::vector<QRect> candidateRects_;
    mutable bool layoutDirty_ = true;
    mutable QSize cachedSize_;
};

} // namespace suyan

#endif // SUYAN_UI_CANDIDATE_VIEW_H
