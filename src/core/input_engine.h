/**
 * InputEngine - 输入引擎
 *
 * 核心协调模块，管理输入状态、处理按键事件、协调各组件。
 * 连接 RimeWrapper（输入法引擎）和 PlatformBridge（平台层）。
 */

#ifndef SUYAN_CORE_INPUT_ENGINE_H
#define SUYAN_CORE_INPUT_ENGINE_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "rime_api.h"

namespace suyan {

// 前向声明
class IPlatformBridge;
class RimeWrapper;
class FrequencyManager;
struct CandidateMenu;

/**
 * 输入模式枚举
 */
enum class InputMode {
    Chinese,        // 中文模式
    English,        // 英文模式
    TempEnglish     // 临时英文模式（大写字母开头）
};

/**
 * 候选词结构（UI 层使用）
 */
struct InputCandidate {
    std::string text;       // 候选词文本
    std::string comment;    // 注释（如拼音）
    int index;              // 序号 (1-based，用于显示)
};

/**
 * 输入状态结构
 *
 * 包含当前输入的完整状态，用于更新 UI。
 */
struct InputState {
    std::string preedit;                    // 当前输入的拼音（带分隔符）
    std::string rawInput;                   // 原始输入（不带分隔符）
    std::vector<InputCandidate> candidates; // 候选词列表（当前页）
    int highlightedIndex;                   // 当前高亮的候选词索引 (0-based)
    int pageIndex;                          // 当前页码 (0-based)
    int pageSize;                           // 每页候选词数量
    bool hasMorePages;                      // 是否有更多页
    InputMode mode;                         // 输入模式
    bool isComposing;                       // 是否正在输入
    
    // 多行展开模式（搜狗风格）
    bool isExpanded;                        // 是否展开多行
    int expandedRows;                       // 展开的行数（1-5）
    int currentRow;                         // 当前选中的行 (0-based)
    int currentCol;                         // 当前选中的列 (0-based)
    int totalCandidates;                    // 总候选词数量（用于多行显示）
};

/**
 * 状态变化回调类型
 */
using StateChangedCallback = std::function<void(const InputState& state)>;

/**
 * 文字提交回调类型
 */
using CommitTextCallback = std::function<void(const std::string& text)>;

/**
 * 按键修饰符掩码
 */
namespace KeyModifier {
    constexpr int None    = 0;
    constexpr int Shift   = 1 << 0;
    constexpr int Control = 1 << 2;
    constexpr int Alt     = 1 << 3;
    constexpr int Super   = 1 << 6;  // Command (macOS) / Win (Windows)
}

/**
 * 常用键码定义
 */
namespace KeyCode {
    constexpr int Escape    = 0xff1b;
    constexpr int Return    = 0xff0d;
    constexpr int BackSpace = 0xff08;
    constexpr int Delete    = 0xffff;
    constexpr int Tab       = 0xff09;
    constexpr int Space     = 0x0020;
    constexpr int PageUp    = 0xff55;
    constexpr int PageDown  = 0xff56;
    constexpr int Home      = 0xff50;
    constexpr int End       = 0xff57;
    constexpr int Left      = 0xff51;
    constexpr int Up        = 0xff52;
    constexpr int Right     = 0xff53;
    constexpr int Down      = 0xff54;
    constexpr int ShiftL    = 0xffe1;
    constexpr int ShiftR    = 0xffe2;
    constexpr int ControlL  = 0xffe3;
    constexpr int ControlR  = 0xffe4;
    constexpr int Minus     = 0x002d;  // '-'
    constexpr int Equal     = 0x003d;  // '='
    constexpr int BracketL  = 0x005b;  // '['
    constexpr int BracketR  = 0x005d;  // ']'
}

/**
 * InputEngine - 输入引擎类
 *
 * 职责：
 * 1. 管理输入状态（模式、preedit、候选词）
 * 2. 处理按键事件并分发到 RimeWrapper
 * 3. 处理候选词选择和翻页
 * 4. 处理中英文模式切换
 * 5. 通知 UI 层状态变化
 */
class InputEngine {
public:
    InputEngine();
    ~InputEngine();

    // 禁止拷贝
    InputEngine(const InputEngine&) = delete;
    InputEngine& operator=(const InputEngine&) = delete;

    /**
     * 初始化输入引擎
     *
     * @param userDataDir 用户数据目录
     * @param sharedDataDir 共享数据目录
     * @return 是否成功
     */
    bool initialize(const std::string& userDataDir,
                    const std::string& sharedDataDir);

    /**
     * 关闭输入引擎
     */
    void shutdown();

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    // ========== 平台桥接 ==========

    /**
     * 设置平台桥接
     *
     * @param bridge 平台桥接实例（不转移所有权）
     */
    void setPlatformBridge(IPlatformBridge* bridge);

    /**
     * 获取平台桥接
     */
    IPlatformBridge* getPlatformBridge() const { return platformBridge_; }

    // ========== 按键处理 ==========

    /**
     * 处理按键事件
     *
     * @param keyCode 键码
     * @param modifiers 修饰键掩码
     * @return true 表示按键已处理，false 表示应传递给应用
     */
    bool processKeyEvent(int keyCode, int modifiers);

    // ========== 候选词操作 ==========

    /**
     * 选择候选词
     *
     * @param index 候选词索引 (1-9 对应当前页的候选词)
     * @return 是否成功
     */
    bool selectCandidate(int index);

    /**
     * 向前翻页
     *
     * @return 是否成功
     */
    bool pageUp();

    /**
     * 向后翻页
     *
     * @return 是否成功
     */
    bool pageDown();

    // ========== 模式切换 ==========

    /**
     * 切换输入模式（中文/英文）
     */
    void toggleMode();

    /**
     * 设置输入模式
     *
     * @param mode 目标模式
     */
    void setMode(InputMode mode);

    /**
     * 获取当前输入模式
     */
    InputMode getMode() const { return mode_; }

    // ========== 状态管理 ==========

    /**
     * 获取当前输入状态
     */
    InputState getState() const;

    /**
     * 重置输入状态（清空输入、关闭候选词窗口）
     */
    void reset();

    /**
     * 提交当前输入（提交首选候选词或原始拼音）
     */
    void commit();

    /**
     * 检查是否正在输入
     */
    bool isComposing() const;

    // ========== 回调设置 ==========

    /**
     * 设置状态变化回调
     */
    void setStateChangedCallback(StateChangedCallback callback);

    /**
     * 设置文字提交回调
     */
    void setCommitTextCallback(CommitTextCallback callback);

    // ========== 词频学习 ==========

    /**
     * 启用/禁用词频学习
     *
     * @param enabled 是否启用
     */
    void setFrequencyLearningEnabled(bool enabled);

    /**
     * 检查词频学习是否启用
     */
    bool isFrequencyLearningEnabled() const { return frequencyLearningEnabled_; }

    /**
     * 设置词频排序的最小阈值
     *
     * @param minFrequency 最小词频（低于此值的词不参与排序提升）
     */
    void setMinFrequencyForSorting(int minFrequency);

    /**
     * 获取词频排序的最小阈值
     */
    int getMinFrequencyForSorting() const { return minFrequencyForSorting_; }

    // ========== 激活/停用 ==========

    /**
     * 激活输入引擎（输入法被选中时调用）
     */
    void activate();

    /**
     * 停用输入引擎（切换到其他输入法时调用）
     */
    void deactivate();

private:
    // 内部方法
    void updateState();
    void notifyStateChanged();
    void notifyCommitText(const std::string& text);
    bool handleEnglishMode(int keyCode, int modifiers);
    bool handleChineseMode(int keyCode, int modifiers);
    bool handleTempEnglishMode(int keyCode, int modifiers);
    bool handleArrowKeys(int keyCode);  // 处理箭头键导航
    bool isAlphaKey(int keyCode) const;
    bool isDigitKey(int keyCode) const;
    bool isPunctuationKey(int keyCode) const;
    bool shouldEnterTempEnglish(int keyCode, int modifiers) const;
    void exitTempEnglishMode();
    void commitTempEnglishBuffer();
    void resetExpandedState();  // 重置展开状态
    
    // 词频学习相关
    void updateFrequencyForSelectedCandidate(const std::string& text, const std::string& pinyin);
    std::vector<InputCandidate> applySortingWithUserFrequency(
        const std::vector<InputCandidate>& candidates,
        const std::string& pinyin) const;

    // 成员变量
    bool initialized_ = false;
    bool active_ = false;
    InputMode mode_ = InputMode::Chinese;
    RimeSessionId sessionId_ = 0;
    IPlatformBridge* platformBridge_ = nullptr;

    // 临时英文模式缓冲区
    std::string tempEnglishBuffer_;
    
    // 词频学习设置
    // 注意：已禁用自定义词频学习，因为它会导致显示和实际选择不一致
    // RIME 本身有词频学习功能，会根据用户选择自动调整候选词顺序
    bool frequencyLearningEnabled_ = false;
    int minFrequencyForSorting_ = 1;
    
    // 多行展开状态（搜狗风格导航）
    bool isExpanded_ = false;           // 是否展开多行
    int expandedRows_ = 1;              // 展开的行数（1-5）
    int currentRow_ = 0;                // 当前选中的行 (0-based)
    int currentCol_ = 0;                // 当前选中的列 (0-based)
    std::vector<InputCandidate> expandedCandidates_;  // 展开模式下的所有候选词
    
    // 数字后标点智能转换
    char lastCommittedChar_ = 0;        // 上一个提交的字符（用于判断数字后的标点）

    // 回调
    StateChangedCallback stateChangedCallback_;
    CommitTextCallback commitTextCallback_;

    // 缓存的状态
    mutable InputState cachedState_;
};

} // namespace suyan

#endif // SUYAN_CORE_INPUT_ENGINE_H
