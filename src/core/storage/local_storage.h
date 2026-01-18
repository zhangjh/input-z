// src/core/storage/local_storage.h
// 本地扩展存储接口定义

#ifndef IME_LOCAL_STORAGE_H
#define IME_LOCAL_STORAGE_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ime {
namespace storage {

// 词库类型枚举
enum class DictType {
    Base,      // 基础词库
    Extended,  // 扩展词库
    Industry,  // 行业词库
    User       // 用户词库
};

// 本地词库元数据
struct LocalDictionaryMeta {
    std::string id;            // 词库唯一标识
    std::string name;          // 词库名称
    std::string type;          // 词库类型 (base, extended, industry, user)
    std::string localVersion;  // 本地版本
    std::string cloudVersion;  // 云端最新版本（可能为空）
    int64_t wordCount;         // 词条数量
    std::string filePath;      // 文件路径
    std::string checksum;      // 校验和
    int priority;              // 优先级，数值越大优先级越高
    bool isEnabled;            // 是否启用
    int64_t installedAt;       // 安装时间戳
    int64_t updatedAt;         // 更新时间戳
};

// 用户词频数据
struct WordFrequency {
    std::string word;     // 词
    std::string pinyin;   // 拼音
    int frequency;        // 频率
};

// 下载任务状态
enum class DownloadStatus {
    Pending,     // 等待中
    Downloading, // 下载中
    Paused,      // 已暂停
    Completed,   // 已完成
    Failed       // 失败
};

// 下载任务
struct DownloadTask {
    int64_t id;                  // 任务 ID
    std::string dictionaryId;    // 词库 ID
    std::string version;         // 版本
    std::string downloadUrl;     // 下载 URL
    int64_t totalSize;           // 总大小
    int64_t downloadedSize;      // 已下载大小
    std::string tempFilePath;    // 临时文件路径
    DownloadStatus status;       // 状态
    std::string errorMessage;    // 错误信息
    int64_t createdAt;           // 创建时间戳
    int64_t updatedAt;           // 更新时间戳
};

// 本地存储接口
class ILocalStorage {
public:
    virtual ~ILocalStorage() = default;

    // ==================== 数据库生命周期 ====================

    // 初始化数据库（创建表结构）
    virtual bool Initialize() = 0;

    // 关闭数据库连接
    virtual void Close() = 0;

    // 检查数据库是否已初始化
    virtual bool IsInitialized() const = 0;

    // ==================== 词库元数据操作 ====================

    // 保存词库元数据（插入或更新）
    virtual bool SaveDictionaryMeta(const LocalDictionaryMeta& meta) = 0;

    // 获取词库元数据
    virtual std::optional<LocalDictionaryMeta> GetDictionaryMeta(
        const std::string& dictId) = 0;

    // 获取所有词库元数据
    virtual std::vector<LocalDictionaryMeta> GetAllDictionaries() = 0;

    // 获取已启用的词库列表（按优先级排序）
    virtual std::vector<LocalDictionaryMeta> GetEnabledDictionaries() = 0;

    // 更新词库版本
    virtual bool UpdateDictionaryVersion(const std::string& dictId,
                                         const std::string& localVersion,
                                         const std::string& cloudVersion = "") = 0;

    // 更新词库启用状态
    virtual bool SetDictionaryEnabled(const std::string& dictId,
                                      bool enabled) = 0;

    // 更新词库优先级
    virtual bool SetDictionaryPriority(const std::string& dictId,
                                       int priority) = 0;

    // 删除词库元数据
    virtual bool DeleteDictionaryMeta(const std::string& dictId) = 0;

    // ==================== 词频操作 ====================

    // 增加词频（如果不存在则创建）
    virtual bool IncrementWordFrequency(const std::string& word,
                                        const std::string& pinyin) = 0;

    // 获取词频
    virtual int GetWordFrequency(const std::string& word,
                                 const std::string& pinyin) = 0;

    // 获取指定拼音的高频词列表
    virtual std::vector<WordFrequency> GetTopFrequencyWords(
        const std::string& pinyin, int limit = 10) = 0;

    // 获取所有词频数据
    virtual std::vector<WordFrequency> GetAllWordFrequencies() = 0;

    // 删除词频记录
    virtual bool DeleteWordFrequency(const std::string& word,
                                     const std::string& pinyin) = 0;

    // 清空所有词频数据
    virtual bool ClearAllWordFrequencies() = 0;

    // 清理低频词（删除 frequency <= threshold 且超过 days 天未更新的记录）
    virtual int CleanupLowFrequencyWords(int frequencyThreshold = 1,
                                         int daysThreshold = 30) = 0;

    // 限制词频记录总量（保留高频词，删除低频词）
    virtual int EnforceFrequencyLimit(int maxRecords = 50000) = 0;

    // 获取词频记录总数
    virtual int GetWordFrequencyCount() = 0;

    // ==================== 配置操作 ====================

    // 获取配置值
    virtual std::string GetConfig(const std::string& key,
                                  const std::string& defaultValue = "") = 0;

    // 设置配置值
    virtual bool SetConfig(const std::string& key, const std::string& value) = 0;

    // 删除配置
    virtual bool DeleteConfig(const std::string& key) = 0;

    // 获取所有配置
    virtual std::vector<std::pair<std::string, std::string>> GetAllConfigs() = 0;

    // ==================== 下载任务操作 ====================

    // 保存下载任务
    virtual bool SaveDownloadTask(const DownloadTask& task) = 0;

    // 获取下载任务
    virtual std::optional<DownloadTask> GetDownloadTask(
        const std::string& dictId) = 0;

    // 更新下载进度
    virtual bool UpdateDownloadProgress(const std::string& dictId,
                                        int64_t downloadedSize,
                                        DownloadStatus status) = 0;

    // 删除下载任务
    virtual bool DeleteDownloadTask(const std::string& dictId) = 0;

    // 获取所有未完成的下载任务
    virtual std::vector<DownloadTask> GetPendingDownloadTasks() = 0;
};

}  // namespace storage
}  // namespace ime

#endif  // IME_LOCAL_STORAGE_H
