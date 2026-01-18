// src/platform/macos/squirrel_integration.h
// Squirrel 集成层 - 将我们的核心模块集成到 Squirrel 前端 (macOS)

#ifndef IME_SQUIRREL_INTEGRATION_H
#define IME_SQUIRREL_INTEGRATION_H

#ifdef PLATFORM_MACOS

#include <memory>
#include <string>
#include <vector>
#include <functional>

// 前向声明
namespace ime {
namespace storage {
class ILocalStorage;
class SqliteStorage;
}
namespace frequency {
class IFrequencyManager;
class FrequencyManagerImpl;
}
namespace input {
class CandidateMerger;
class InputSession;
struct CandidateWord;
enum class InputMode;
}
namespace learning {
class IAutoLearner;
class AutoLearnerImpl;
}
}

namespace ime {
namespace platform {
namespace macos {

// Squirrel 集成配置
struct SquirrelIntegrationConfig {
    std::string userDataPath;       // 用户数据目录 (~/Library/Rime)
    std::string sharedDataPath;     // 共享数据目录 (/Library/Input Methods/Squirrel.app/Contents/SharedSupport)
    std::string logPath;            // 日志目录
    bool enableCloudSync;           // 是否启用云端同步
    bool enableAutoLearn;           // 是否启用自动学词
    int pageSize;                   // 每页候选词数量

    // 默认配置
    static SquirrelIntegrationConfig Default();
};

// Squirrel 集成管理器
// 负责将我们的核心模块（词频管理、候选词合并、自动学词等）集成到 Squirrel
class SquirrelIntegration {
public:
    // 获取单例实例
    static SquirrelIntegration& Instance();

    // 禁止拷贝
    SquirrelIntegration(const SquirrelIntegration&) = delete;
    SquirrelIntegration& operator=(const SquirrelIntegration&) = delete;

    // ==================== 生命周期 ====================

    // 初始化集成层
    // @param config 配置
    // @return 是否成功
    bool Initialize(const SquirrelIntegrationConfig& config);

    // 关闭集成层
    void Shutdown();

    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

    // ==================== 候选词处理 ====================

    // 合并用户高频词到 librime 候选词列表
    // @param rimeCandidates librime 返回的候选词（文本列表）
    // @param pinyin 当前输入的拼音
    // @return 合并后的候选词列表
    std::vector<std::string> MergeCandidates(
        const std::vector<std::string>& rimeCandidates,
        const std::string& pinyin);

    // 获取用户高频词
    // @param pinyin 拼音
    // @param limit 返回数量限制
    // @return 用户高频词列表
    std::vector<std::string> GetUserTopWords(const std::string& pinyin,
                                              int limit = 5);

    // ==================== 词频记录 ====================

    // 记录用户选择了某个候选词
    // @param word 选择的词
    // @param pinyin 对应的拼音
    void RecordWordSelection(const std::string& word,
                             const std::string& pinyin);

    // 记录连续选择（用于自动学词）
    // @param word 选择的词
    // @param pinyin 对应的拼音
    void RecordConsecutiveSelection(const std::string& word,
                                    const std::string& pinyin);

    // 提交完成（触发自动学词检查）
    void OnCommitComplete();

    // ==================== 输入模式 ====================

    // 获取当前输入模式
    input::InputMode GetInputMode() const;

    // 设置输入模式
    void SetInputMode(input::InputMode mode);

    // 切换输入模式
    void ToggleInputMode();

    // ==================== 配置 ====================

    // 获取配置值
    std::string GetConfig(const std::string& key,
                          const std::string& defaultValue = "");

    // 设置配置值
    bool SetConfig(const std::string& key, const std::string& value);

    // ==================== 词库管理 ====================

    // 获取已启用的词库列表
    std::vector<std::string> GetEnabledDictionaries();

    // 设置词库启用状态
    bool SetDictionaryEnabled(const std::string& dictId, bool enabled);

    // ==================== 词频维护 ====================

    // 执行词频清理（删除低频旧词 + 限制总量）
    // @return 删除的记录数
    int CleanupWordFrequencies();

private:
    // 私有构造函数（单例）
    SquirrelIntegration();
    ~SquirrelIntegration();

    // 初始化存储层
    bool InitializeStorage(const std::string& userDataPath);

    // 初始化词频管理器
    bool InitializeFrequencyManager();

    // 初始化自动学词
    bool InitializeAutoLearner();

private:
    bool initialized_;
    SquirrelIntegrationConfig config_;

    // 核心模块
    std::unique_ptr<storage::SqliteStorage> storage_;
    std::unique_ptr<frequency::FrequencyManagerImpl> frequencyManager_;
    std::unique_ptr<input::CandidateMerger> candidateMerger_;
    std::unique_ptr<learning::AutoLearnerImpl> autoLearner_;

    // 当前输入模式
    input::InputMode currentMode_;
};

}  // namespace macos
}  // namespace platform
}  // namespace ime

// ==================== C 接口（供 Swift 通过 Bridging Header 调用）====================

#ifdef __cplusplus
extern "C" {
#endif

// 初始化集成层
// @param userDataPath 用户数据目录（UTF-8）
// @param sharedDataPath 共享数据目录（UTF-8）
// @return 0 成功，非 0 失败
int ImeIntegration_Initialize(const char* userDataPath,
                               const char* sharedDataPath);

// 关闭集成层
void ImeIntegration_Shutdown(void);

// 检查是否已初始化
int ImeIntegration_IsInitialized(void);

// 合并候选词
// @param candidates 候选词数组（以 NULL 结尾）
// @param pinyin 拼音（UTF-8）
// @param outBuffer 输出缓冲区（调用者分配）
// @param bufferSize 缓冲区大小
// @return 合并后的候选词数量
int ImeIntegration_MergeCandidates(const char** candidates,
                                    const char* pinyin,
                                    char** outBuffer,
                                    int bufferSize);

// 释放合并候选词返回的内存
void ImeIntegration_FreeMergedCandidates(char** buffer, int count);

// 记录词选择
// @param word 选择的词（UTF-8）
// @param pinyin 拼音（UTF-8）
void ImeIntegration_RecordSelection(const char* word, const char* pinyin);

// 记录连续选择（用于自动学词）
void ImeIntegration_RecordConsecutive(const char* word, const char* pinyin);

// 提交完成
void ImeIntegration_OnCommit(void);

// 获取输入模式
// @return 0=中文, 1=英文, 2=临时英文
int ImeIntegration_GetInputMode(void);

// 设置输入模式
void ImeIntegration_SetInputMode(int mode);

// 切换输入模式
void ImeIntegration_ToggleInputMode(void);

// 获取配置值
// @param key 配置键
// @param defaultValue 默认值
// @param outBuffer 输出缓冲区
// @param bufferSize 缓冲区大小
// @return 实际长度
int ImeIntegration_GetConfig(const char* key, 
                              const char* defaultValue,
                              char* outBuffer, 
                              int bufferSize);

// 设置配置值
// @return 0 成功，非 0 失败
int ImeIntegration_SetConfig(const char* key, const char* value);

// 获取用户高频词
// @param pinyin 拼音
// @param limit 返回数量限制
// @param outBuffer 输出缓冲区（调用者分配，每个元素需要 free）
// @param bufferSize 缓冲区大小
// @return 实际返回的词数量
int ImeIntegration_GetUserTopWords(const char* pinyin,
                                    int limit,
                                    char** outBuffer,
                                    int bufferSize);

// 释放 GetUserTopWords 返回的内存
void ImeIntegration_FreeUserTopWords(char** buffer, int count);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PLATFORM_MACOS

#endif  // IME_SQUIRREL_INTEGRATION_H
