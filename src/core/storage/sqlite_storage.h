// src/core/storage/sqlite_storage.h
// SQLite 存储实现

#ifndef IME_SQLITE_STORAGE_H
#define IME_SQLITE_STORAGE_H

#include "local_storage.h"

#include <memory>
#include <mutex>
#include <string>

// 前向声明 SQLite 类型
struct sqlite3;
struct sqlite3_stmt;

namespace ime {
namespace storage {

// SQLite 存储实现
class SqliteStorage : public ILocalStorage {
public:
    // 构造函数
    // @param dbPath 数据库文件路径，如果为空则使用默认路径
    explicit SqliteStorage(const std::string& dbPath = "");

    // 析构函数
    ~SqliteStorage() override;

    // 禁止拷贝
    SqliteStorage(const SqliteStorage&) = delete;
    SqliteStorage& operator=(const SqliteStorage&) = delete;

    // 允许移动
    SqliteStorage(SqliteStorage&& other) noexcept;
    SqliteStorage& operator=(SqliteStorage&& other) noexcept;

    // ==================== ILocalStorage 接口实现 ====================

    bool Initialize() override;
    void Close() override;
    bool IsInitialized() const override;

    // 词库元数据操作
    bool SaveDictionaryMeta(const LocalDictionaryMeta& meta) override;
    std::optional<LocalDictionaryMeta> GetDictionaryMeta(
        const std::string& dictId) override;
    std::vector<LocalDictionaryMeta> GetAllDictionaries() override;
    std::vector<LocalDictionaryMeta> GetEnabledDictionaries() override;
    bool UpdateDictionaryVersion(const std::string& dictId,
                                 const std::string& localVersion,
                                 const std::string& cloudVersion = "") override;
    bool SetDictionaryEnabled(const std::string& dictId, bool enabled) override;
    bool SetDictionaryPriority(const std::string& dictId, int priority) override;
    bool DeleteDictionaryMeta(const std::string& dictId) override;

    // 词频操作
    bool IncrementWordFrequency(const std::string& word,
                                const std::string& pinyin) override;
    int GetWordFrequency(const std::string& word,
                         const std::string& pinyin) override;
    std::vector<WordFrequency> GetTopFrequencyWords(const std::string& pinyin,
                                                    int limit = 10) override;
    std::vector<WordFrequency> GetAllWordFrequencies() override;
    bool DeleteWordFrequency(const std::string& word,
                             const std::string& pinyin) override;
    bool ClearAllWordFrequencies() override;
    int CleanupLowFrequencyWords(int frequencyThreshold = 1,
                                 int daysThreshold = 30) override;
    int EnforceFrequencyLimit(int maxRecords = 50000) override;
    int GetWordFrequencyCount() override;

    // 配置操作
    std::string GetConfig(const std::string& key,
                          const std::string& defaultValue = "") override;
    bool SetConfig(const std::string& key, const std::string& value) override;
    bool DeleteConfig(const std::string& key) override;
    std::vector<std::pair<std::string, std::string>> GetAllConfigs() override;

    // 下载任务操作
    bool SaveDownloadTask(const DownloadTask& task) override;
    std::optional<DownloadTask> GetDownloadTask(
        const std::string& dictId) override;
    bool UpdateDownloadProgress(const std::string& dictId,
                                int64_t downloadedSize,
                                DownloadStatus status) override;
    bool DeleteDownloadTask(const std::string& dictId) override;
    std::vector<DownloadTask> GetPendingDownloadTasks() override;

    // ==================== 辅助方法 ====================

    // 获取数据库文件路径
    const std::string& GetDbPath() const { return dbPath_; }

    // 获取最后一次错误信息
    std::string GetLastError() const;

    // 执行数据库事务
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

private:
    // 创建数据库表结构
    bool CreateTables();

    // 插入默认配置
    bool InsertDefaultConfigs();

    // 执行 SQL 语句（无返回值）
    bool ExecuteSql(const std::string& sql);

    // 准备 SQL 语句
    sqlite3_stmt* PrepareStatement(const std::string& sql);

    // 释放语句
    void FinalizeStatement(sqlite3_stmt* stmt);

    // 获取当前时间戳
    static int64_t GetCurrentTimestamp();

    // 下载状态转换
    static std::string DownloadStatusToString(DownloadStatus status);
    static DownloadStatus StringToDownloadStatus(const std::string& str);

private:
    std::string dbPath_;           // 数据库文件路径
    sqlite3* db_;                  // SQLite 数据库句柄
    mutable std::mutex mutex_;     // 线程安全锁
    bool initialized_;             // 是否已初始化
};

}  // namespace storage
}  // namespace ime

#endif  // IME_SQLITE_STORAGE_H
