// src/core/storage/sqlite_storage.cpp
// SQLite 存储实现

#include "sqlite_storage.h"

#include <chrono>
#include <cstring>
#include <sqlite3.h>
#include <sstream>

namespace ime {
namespace storage {

// SQL 语句定义
namespace sql {

// 创建词库元数据表
constexpr const char* CREATE_DICTIONARIES_TABLE = R"(
CREATE TABLE IF NOT EXISTS local_dictionaries (
    id VARCHAR(64) PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    type VARCHAR(32) NOT NULL,
    local_version VARCHAR(32) NOT NULL,
    cloud_version VARCHAR(32),
    word_count INTEGER NOT NULL,
    file_path VARCHAR(256) NOT NULL,
    checksum VARCHAR(64) NOT NULL,
    priority INTEGER DEFAULT 0,
    is_enabled INTEGER DEFAULT 1,
    installed_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);
)";

// 创建词频表
constexpr const char* CREATE_FREQUENCY_TABLE = R"(
CREATE TABLE IF NOT EXISTS user_word_frequency (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    word TEXT NOT NULL,
    pinyin TEXT NOT NULL,
    frequency INTEGER DEFAULT 1,
    updated_at INTEGER DEFAULT (strftime('%s', 'now')),
    UNIQUE(word, pinyin)
);
)";

// 创建词频索引
constexpr const char* CREATE_FREQUENCY_INDEX_PINYIN = R"(
CREATE INDEX IF NOT EXISTS idx_word_frequency_pinyin 
ON user_word_frequency(pinyin);
)";

constexpr const char* CREATE_FREQUENCY_INDEX_FREQ = R"(
CREATE INDEX IF NOT EXISTS idx_word_frequency_freq 
ON user_word_frequency(frequency DESC);
)";

// 创建下载任务表
constexpr const char* CREATE_DOWNLOAD_TASKS_TABLE = R"(
CREATE TABLE IF NOT EXISTS download_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    dictionary_id VARCHAR(64) NOT NULL,
    version VARCHAR(32) NOT NULL,
    download_url TEXT NOT NULL,
    total_size INTEGER NOT NULL,
    downloaded_size INTEGER DEFAULT 0,
    temp_file_path VARCHAR(256),
    status VARCHAR(32) DEFAULT 'pending',
    error_message TEXT,
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);
)";

// 创建配置表
constexpr const char* CREATE_CONFIG_TABLE = R"(
CREATE TABLE IF NOT EXISTS app_config (
    key VARCHAR(128) PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);
)";

}  // namespace sql

// 构造函数
SqliteStorage::SqliteStorage(const std::string& dbPath)
    : dbPath_(dbPath), db_(nullptr), initialized_(false) {
    if (dbPath_.empty()) {
        // 使用默认路径
        dbPath_ = "ime_data.db";
    }
}

// 析构函数
SqliteStorage::~SqliteStorage() {
    Close();
}

// 移动构造函数
SqliteStorage::SqliteStorage(SqliteStorage&& other) noexcept
    : dbPath_(std::move(other.dbPath_)),
      db_(other.db_),
      initialized_(other.initialized_) {
    other.db_ = nullptr;
    other.initialized_ = false;
}

// 移动赋值运算符
SqliteStorage& SqliteStorage::operator=(SqliteStorage&& other) noexcept {
    if (this != &other) {
        Close();
        dbPath_ = std::move(other.dbPath_);
        db_ = other.db_;
        initialized_ = other.initialized_;
        other.db_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

// 初始化数据库
bool SqliteStorage::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    // 打开数据库
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // 启用外键约束
    ExecuteSql("PRAGMA foreign_keys = ON;");

    // 设置 WAL 模式以提高并发性能
    ExecuteSql("PRAGMA journal_mode = WAL;");

    // 创建表结构
    if (!CreateTables()) {
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // 插入默认配置
    InsertDefaultConfigs();

    initialized_ = true;
    return true;
}

// 关闭数据库
void SqliteStorage::Close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    initialized_ = false;
}

// 检查是否已初始化
bool SqliteStorage::IsInitialized() const {
    return initialized_;
}

// 创建表结构
bool SqliteStorage::CreateTables() {
    // 创建词库元数据表
    if (!ExecuteSql(sql::CREATE_DICTIONARIES_TABLE)) {
        return false;
    }

    // 创建词频表
    if (!ExecuteSql(sql::CREATE_FREQUENCY_TABLE)) {
        return false;
    }

    // 创建词频索引
    if (!ExecuteSql(sql::CREATE_FREQUENCY_INDEX_PINYIN)) {
        return false;
    }
    if (!ExecuteSql(sql::CREATE_FREQUENCY_INDEX_FREQ)) {
        return false;
    }

    // 创建下载任务表
    if (!ExecuteSql(sql::CREATE_DOWNLOAD_TASKS_TABLE)) {
        return false;
    }

    // 创建配置表
    if (!ExecuteSql(sql::CREATE_CONFIG_TABLE)) {
        return false;
    }

    return true;
}

// 插入默认配置
bool SqliteStorage::InsertDefaultConfigs() {
    const char* defaultConfigs[][2] = {
        {"cloud.enabled", "true"},
        {"cloud.server_url", "https://dict.example.com"},
        {"cloud.check_interval", "86400"},
        {"cloud.auto_update", "true"},
        {"input.default_mode", "chinese"},
        {"input.page_size", "9"},
    };

    const char* sql =
        "INSERT OR IGNORE INTO app_config (key, value) VALUES (?, ?);";

    for (const auto& config : defaultConfigs) {
        sqlite3_stmt* stmt = PrepareStatement(sql);
        if (!stmt) {
            continue;
        }

        sqlite3_bind_text(stmt, 1, config[0], -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, config[1], -1, SQLITE_STATIC);

        sqlite3_step(stmt);
        FinalizeStatement(stmt);
    }

    return true;
}

// 执行 SQL 语句
bool SqliteStorage::ExecuteSql(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        return false;
    }

    return true;
}

// 准备 SQL 语句
sqlite3_stmt* SqliteStorage::PrepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return nullptr;
    }

    return stmt;
}

// 释放语句
void SqliteStorage::FinalizeStatement(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

// 获取当前时间戳
int64_t SqliteStorage::GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// 获取最后一次错误信息
std::string SqliteStorage::GetLastError() const {
    if (db_) {
        return sqlite3_errmsg(db_);
    }
    return "Database not opened";
}

// 事务操作
bool SqliteStorage::BeginTransaction() {
    return ExecuteSql("BEGIN TRANSACTION;");
}

bool SqliteStorage::CommitTransaction() {
    return ExecuteSql("COMMIT;");
}

bool SqliteStorage::RollbackTransaction() {
    return ExecuteSql("ROLLBACK;");
}

// ==================== 词库元数据操作 ====================

bool SqliteStorage::SaveDictionaryMeta(const LocalDictionaryMeta& meta) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        INSERT OR REPLACE INTO local_dictionaries 
        (id, name, type, local_version, cloud_version, word_count, 
         file_path, checksum, priority, is_enabled, installed_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    int64_t now = GetCurrentTimestamp();
    int64_t installedAt = meta.installedAt > 0 ? meta.installedAt : now;

    sqlite3_bind_text(stmt, 1, meta.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, meta.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, meta.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, meta.localVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, meta.cloudVersion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, meta.wordCount);
    sqlite3_bind_text(stmt, 7, meta.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, meta.checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, meta.priority);
    sqlite3_bind_int(stmt, 10, meta.isEnabled ? 1 : 0);
    sqlite3_bind_int64(stmt, 11, installedAt);
    sqlite3_bind_int64(stmt, 12, now);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

std::optional<LocalDictionaryMeta> SqliteStorage::GetDictionaryMeta(
    const std::string& dictId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    const char* sql = R"(
        SELECT id, name, type, local_version, cloud_version, word_count,
               file_path, checksum, priority, is_enabled, installed_at, updated_at
        FROM local_dictionaries WHERE id = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, dictId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        FinalizeStatement(stmt);
        return std::nullopt;
    }

    LocalDictionaryMeta meta;
    meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    meta.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    meta.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    meta.localVersion =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    const char* cloudVer =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    meta.cloudVersion = cloudVer ? cloudVer : "";

    meta.wordCount = sqlite3_column_int64(stmt, 5);
    meta.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    meta.checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    meta.priority = sqlite3_column_int(stmt, 8);
    meta.isEnabled = sqlite3_column_int(stmt, 9) != 0;
    meta.installedAt = sqlite3_column_int64(stmt, 10);
    meta.updatedAt = sqlite3_column_int64(stmt, 11);

    FinalizeStatement(stmt);
    return meta;
}

std::vector<LocalDictionaryMeta> SqliteStorage::GetAllDictionaries() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LocalDictionaryMeta> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = R"(
        SELECT id, name, type, local_version, cloud_version, word_count,
               file_path, checksum, priority, is_enabled, installed_at, updated_at
        FROM local_dictionaries ORDER BY priority DESC, name ASC;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LocalDictionaryMeta meta;
        meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        meta.localVersion =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        const char* cloudVer =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        meta.cloudVersion = cloudVer ? cloudVer : "";

        meta.wordCount = sqlite3_column_int64(stmt, 5);
        meta.filePath =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        meta.checksum =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        meta.priority = sqlite3_column_int(stmt, 8);
        meta.isEnabled = sqlite3_column_int(stmt, 9) != 0;
        meta.installedAt = sqlite3_column_int64(stmt, 10);
        meta.updatedAt = sqlite3_column_int64(stmt, 11);

        result.push_back(std::move(meta));
    }

    FinalizeStatement(stmt);
    return result;
}

std::vector<LocalDictionaryMeta> SqliteStorage::GetEnabledDictionaries() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LocalDictionaryMeta> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = R"(
        SELECT id, name, type, local_version, cloud_version, word_count,
               file_path, checksum, priority, is_enabled, installed_at, updated_at
        FROM local_dictionaries 
        WHERE is_enabled = 1 
        ORDER BY priority DESC, name ASC;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LocalDictionaryMeta meta;
        meta.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        meta.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        meta.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        meta.localVersion =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        const char* cloudVer =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        meta.cloudVersion = cloudVer ? cloudVer : "";

        meta.wordCount = sqlite3_column_int64(stmt, 5);
        meta.filePath =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        meta.checksum =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        meta.priority = sqlite3_column_int(stmt, 8);
        meta.isEnabled = true;
        meta.installedAt = sqlite3_column_int64(stmt, 10);
        meta.updatedAt = sqlite3_column_int64(stmt, 11);

        result.push_back(std::move(meta));
    }

    FinalizeStatement(stmt);
    return result;
}

bool SqliteStorage::UpdateDictionaryVersion(const std::string& dictId,
                                            const std::string& localVersion,
                                            const std::string& cloudVersion) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    std::string sql;
    if (cloudVersion.empty()) {
        sql = R"(
            UPDATE local_dictionaries 
            SET local_version = ?, updated_at = ?
            WHERE id = ?;
        )";
    } else {
        sql = R"(
            UPDATE local_dictionaries 
            SET local_version = ?, cloud_version = ?, updated_at = ?
            WHERE id = ?;
        )";
    }

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    int64_t now = GetCurrentTimestamp();

    if (cloudVersion.empty()) {
        sqlite3_bind_text(stmt, 1, localVersion.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, now);
        sqlite3_bind_text(stmt, 3, dictId.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, 1, localVersion.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, cloudVersion.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, now);
        sqlite3_bind_text(stmt, 4, dictId.c_str(), -1, SQLITE_TRANSIENT);
    }

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool SqliteStorage::SetDictionaryEnabled(const std::string& dictId,
                                         bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        UPDATE local_dictionaries 
        SET is_enabled = ?, updated_at = ?
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, GetCurrentTimestamp());
    sqlite3_bind_text(stmt, 3, dictId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool SqliteStorage::SetDictionaryPriority(const std::string& dictId,
                                          int priority) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        UPDATE local_dictionaries 
        SET priority = ?, updated_at = ?
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, priority);
    sqlite3_bind_int64(stmt, 2, GetCurrentTimestamp());
    sqlite3_bind_text(stmt, 3, dictId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool SqliteStorage::DeleteDictionaryMeta(const std::string& dictId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = "DELETE FROM local_dictionaries WHERE id = ?;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, dictId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}


// ==================== 词频操作 ====================

bool SqliteStorage::IncrementWordFrequency(const std::string& word,
                                           const std::string& pinyin) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    // 使用 UPSERT 语法：如果存在则更新，否则插入
    const char* sql = R"(
        INSERT INTO user_word_frequency (word, pinyin, frequency, updated_at)
        VALUES (?, ?, 1, strftime('%s', 'now'))
        ON CONFLICT(word, pinyin) DO UPDATE SET
            frequency = frequency + 1,
            updated_at = strftime('%s', 'now');
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pinyin.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

int SqliteStorage::GetWordFrequency(const std::string& word,
                                    const std::string& pinyin) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return 0;
    }

    const char* sql = R"(
        SELECT frequency FROM user_word_frequency 
        WHERE word = ? AND pinyin = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pinyin.c_str(), -1, SQLITE_TRANSIENT);

    int frequency = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        frequency = sqlite3_column_int(stmt, 0);
    }

    FinalizeStatement(stmt);
    return frequency;
}

std::vector<WordFrequency> SqliteStorage::GetTopFrequencyWords(
    const std::string& pinyin, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<WordFrequency> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = R"(
        SELECT word, pinyin, frequency
        FROM user_word_frequency 
        WHERE pinyin = ?
        ORDER BY frequency DESC
        LIMIT ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, pinyin.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        WordFrequency wf;
        wf.word = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        wf.pinyin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        wf.frequency = sqlite3_column_int(stmt, 2);

        result.push_back(std::move(wf));
    }

    FinalizeStatement(stmt);
    return result;
}

std::vector<WordFrequency> SqliteStorage::GetAllWordFrequencies() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<WordFrequency> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = R"(
        SELECT word, pinyin, frequency
        FROM user_word_frequency 
        ORDER BY frequency DESC;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        WordFrequency wf;
        wf.word = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        wf.pinyin = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        wf.frequency = sqlite3_column_int(stmt, 2);

        result.push_back(std::move(wf));
    }

    FinalizeStatement(stmt);
    return result;
}

bool SqliteStorage::DeleteWordFrequency(const std::string& word,
                                        const std::string& pinyin) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql =
        "DELETE FROM user_word_frequency WHERE word = ? AND pinyin = ?;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pinyin.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

bool SqliteStorage::ClearAllWordFrequencies() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    return ExecuteSql("DELETE FROM user_word_frequency;");
}

int SqliteStorage::CleanupLowFrequencyWords(int frequencyThreshold,
                                            int daysThreshold) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return 0;
    }

    // 删除 frequency <= threshold 且超过 days 天未更新的记录
    const char* sql = R"(
        DELETE FROM user_word_frequency 
        WHERE frequency <= ? 
        AND updated_at < strftime('%s', 'now') - ? * 86400;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, frequencyThreshold);
    sqlite3_bind_int(stmt, 2, daysThreshold);

    int rc = sqlite3_step(stmt);
    int deletedCount = 0;
    if (rc == SQLITE_DONE) {
        deletedCount = sqlite3_changes(db_);
    }

    FinalizeStatement(stmt);
    return deletedCount;
}

int SqliteStorage::EnforceFrequencyLimit(int maxRecords) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return 0;
    }

    // 获取当前记录数
    const char* countSql = "SELECT COUNT(*) FROM user_word_frequency;";
    sqlite3_stmt* countStmt = PrepareStatement(countSql);
    if (!countStmt) {
        return 0;
    }

    int currentCount = 0;
    if (sqlite3_step(countStmt) == SQLITE_ROW) {
        currentCount = sqlite3_column_int(countStmt, 0);
    }
    FinalizeStatement(countStmt);

    if (currentCount <= maxRecords) {
        return 0;  // 未超限，无需清理
    }

    // 删除超出限制的低频词（保留高频词）
    // 按 frequency DESC 排序，保留前 maxRecords 条
    const char* deleteSql = R"(
        DELETE FROM user_word_frequency 
        WHERE id NOT IN (
            SELECT id FROM user_word_frequency 
            ORDER BY frequency DESC 
            LIMIT ?
        );
    )";

    sqlite3_stmt* deleteStmt = PrepareStatement(deleteSql);
    if (!deleteStmt) {
        return 0;
    }

    sqlite3_bind_int(deleteStmt, 1, maxRecords);

    int rc = sqlite3_step(deleteStmt);
    int deletedCount = 0;
    if (rc == SQLITE_DONE) {
        deletedCount = sqlite3_changes(db_);
    }

    FinalizeStatement(deleteStmt);
    return deletedCount;
}

int SqliteStorage::GetWordFrequencyCount() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return 0;
    }

    const char* sql = "SELECT COUNT(*) FROM user_word_frequency;";
    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    FinalizeStatement(stmt);
    return count;
}

// ==================== 配置操作 ====================

std::string SqliteStorage::GetConfig(const std::string& key,
                                     const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return defaultValue;
    }

    const char* sql = "SELECT value FROM app_config WHERE key = ?;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return defaultValue;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::string result = defaultValue;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* value =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (value) {
            result = value;
        }
    }

    FinalizeStatement(stmt);
    return result;
}

bool SqliteStorage::SetConfig(const std::string& key,
                              const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        INSERT OR REPLACE INTO app_config (key, value, updated_at)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, GetCurrentTimestamp());

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

bool SqliteStorage::DeleteConfig(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = "DELETE FROM app_config WHERE key = ?;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

std::vector<std::pair<std::string, std::string>> SqliteStorage::GetAllConfigs() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::pair<std::string, std::string>> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = "SELECT key, value FROM app_config ORDER BY key;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* key =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* value =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        if (key && value) {
            result.emplace_back(key, value);
        }
    }

    FinalizeStatement(stmt);
    return result;
}

// ==================== 下载任务操作 ====================

std::string SqliteStorage::DownloadStatusToString(DownloadStatus status) {
    switch (status) {
        case DownloadStatus::Pending:
            return "pending";
        case DownloadStatus::Downloading:
            return "downloading";
        case DownloadStatus::Paused:
            return "paused";
        case DownloadStatus::Completed:
            return "completed";
        case DownloadStatus::Failed:
            return "failed";
        default:
            return "pending";
    }
}

DownloadStatus SqliteStorage::StringToDownloadStatus(const std::string& str) {
    if (str == "downloading") {
        return DownloadStatus::Downloading;
    } else if (str == "paused") {
        return DownloadStatus::Paused;
    } else if (str == "completed") {
        return DownloadStatus::Completed;
    } else if (str == "failed") {
        return DownloadStatus::Failed;
    }
    return DownloadStatus::Pending;
}

bool SqliteStorage::SaveDownloadTask(const DownloadTask& task) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        INSERT OR REPLACE INTO download_tasks 
        (dictionary_id, version, download_url, total_size, downloaded_size,
         temp_file_path, status, error_message, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    int64_t now = GetCurrentTimestamp();
    int64_t createdAt = task.createdAt > 0 ? task.createdAt : now;

    sqlite3_bind_text(stmt, 1, task.dictionaryId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.downloadUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, task.totalSize);
    sqlite3_bind_int64(stmt, 5, task.downloadedSize);
    sqlite3_bind_text(stmt, 6, task.tempFilePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, DownloadStatusToString(task.status).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, task.errorMessage.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, createdAt);
    sqlite3_bind_int64(stmt, 10, now);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

std::optional<DownloadTask> SqliteStorage::GetDownloadTask(
    const std::string& dictId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    const char* sql = R"(
        SELECT id, dictionary_id, version, download_url, total_size, 
               downloaded_size, temp_file_path, status, error_message,
               created_at, updated_at
        FROM download_tasks WHERE dictionary_id = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, dictId.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        FinalizeStatement(stmt);
        return std::nullopt;
    }

    DownloadTask task;
    task.id = sqlite3_column_int64(stmt, 0);
    task.dictionaryId =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    task.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    task.downloadUrl =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    task.totalSize = sqlite3_column_int64(stmt, 4);
    task.downloadedSize = sqlite3_column_int64(stmt, 5);

    const char* tempPath =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    task.tempFilePath = tempPath ? tempPath : "";

    const char* statusStr =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    task.status = StringToDownloadStatus(statusStr ? statusStr : "pending");

    const char* errMsg =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    task.errorMessage = errMsg ? errMsg : "";

    task.createdAt = sqlite3_column_int64(stmt, 9);
    task.updatedAt = sqlite3_column_int64(stmt, 10);

    FinalizeStatement(stmt);
    return task;
}

bool SqliteStorage::UpdateDownloadProgress(const std::string& dictId,
                                           int64_t downloadedSize,
                                           DownloadStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = R"(
        UPDATE download_tasks 
        SET downloaded_size = ?, status = ?, updated_at = ?
        WHERE dictionary_id = ?;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, downloadedSize);
    sqlite3_bind_text(stmt, 2, DownloadStatusToString(status).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, GetCurrentTimestamp());
    sqlite3_bind_text(stmt, 4, dictId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool SqliteStorage::DeleteDownloadTask(const std::string& dictId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return false;
    }

    const char* sql = "DELETE FROM download_tasks WHERE dictionary_id = ?;";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, dictId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    FinalizeStatement(stmt);

    return rc == SQLITE_DONE;
}

std::vector<DownloadTask> SqliteStorage::GetPendingDownloadTasks() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<DownloadTask> result;

    if (!initialized_) {
        return result;
    }

    const char* sql = R"(
        SELECT id, dictionary_id, version, download_url, total_size, 
               downloaded_size, temp_file_path, status, error_message,
               created_at, updated_at
        FROM download_tasks 
        WHERE status IN ('pending', 'downloading', 'paused')
        ORDER BY created_at ASC;
    )";

    sqlite3_stmt* stmt = PrepareStatement(sql);
    if (!stmt) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DownloadTask task;
        task.id = sqlite3_column_int64(stmt, 0);
        task.dictionaryId =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        task.version =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        task.downloadUrl =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        task.totalSize = sqlite3_column_int64(stmt, 4);
        task.downloadedSize = sqlite3_column_int64(stmt, 5);

        const char* tempPath =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        task.tempFilePath = tempPath ? tempPath : "";

        const char* statusStr =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        task.status = StringToDownloadStatus(statusStr ? statusStr : "pending");

        const char* errMsg =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        task.errorMessage = errMsg ? errMsg : "";

        task.createdAt = sqlite3_column_int64(stmt, 9);
        task.updatedAt = sqlite3_column_int64(stmt, 10);

        result.push_back(std::move(task));
    }

    FinalizeStatement(stmt);
    return result;
}

}  // namespace storage
}  // namespace ime
