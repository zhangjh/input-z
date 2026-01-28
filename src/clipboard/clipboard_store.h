/**
 * ClipboardStore - 剪贴板存储层
 *
 * 使用 SQLite 存储剪贴板历史记录的元数据。
 * 支持文本内容直接存储，图片内容存储路径引用。
 * 提供 FTS5 全文搜索支持。
 *
 * 功能：
 * - 剪贴板记录的 CRUD 操作
 * - 基于 SHA-256 哈希的内容去重
 * - FTS5 全文搜索（仅文本）
 * - 过期记录清理
 */

#ifndef SUYAN_CLIPBOARD_CLIPBOARD_STORE_H
#define SUYAN_CLIPBOARD_CLIPBOARD_STORE_H

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// 前向声明 SQLite
struct sqlite3;
struct sqlite3_stmt;

namespace suyan {

/**
 * 剪贴板内容类型
 */
enum class ClipboardContentType {
    Text = 0,       // 文本
    Image = 1,      // 图片
    Unknown = 2     // 未知/不支持
};

/**
 * 剪贴板记录结构
 */
struct ClipboardRecord {
    int64_t id = 0;                     // 数据库 ID
    ClipboardContentType type = ClipboardContentType::Unknown;  // 内容类型
    std::string content;                // 文本内容或图片路径
    std::string contentHash;            // SHA-256 哈希
    std::string sourceApp;              // 来源应用 Bundle ID
    std::string thumbnailPath;          // 缩略图路径（图片类型）
    std::string imageFormat;            // 图片格式（图片类型）
    int imageWidth = 0;                 // 图片宽度
    int imageHeight = 0;                // 图片高度
    int64_t fileSize = 0;               // 文件大小（字节）
    int64_t createdAt = 0;              // 创建时间戳（Unix 毫秒）
    int64_t lastUsedAt = 0;             // 最后使用时间戳（Unix 毫秒）
};

/**
 * 添加记录的结果
 */
struct AddRecordResult {
    int64_t id = -1;        // 记录 ID，失败为 -1
    bool isNew = false;     // 是否是新记录（false 表示是去重更新）
};

/**
 * ClipboardStore - 剪贴板存储类
 *
 * 单例模式，管理剪贴板历史记录的 SQLite 存储。
 */
class ClipboardStore {
public:
    /**
     * 获取单例实例
     */
    static ClipboardStore& instance();

    // 禁止拷贝和移动
    ClipboardStore(const ClipboardStore&) = delete;
    ClipboardStore& operator=(const ClipboardStore&) = delete;
    ClipboardStore(ClipboardStore&&) = delete;
    ClipboardStore& operator=(ClipboardStore&&) = delete;

    /**
     * 初始化数据库
     *
     * @param dbPath 数据库文件路径
     * @return 是否成功
     */
    bool initialize(const std::string& dbPath);

    /**
     * 关闭数据库
     */
    void shutdown();

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * 获取数据库路径
     */
    std::string getDatabasePath() const { return dbPath_; }

    // ========== CRUD 操作 ==========

    /**
     * 添加记录
     *
     * 如果哈希值已存在，则更新该记录的 lastUsedAt 时间戳并返回已有 ID。
     *
     * @param record 记录数据（id 字段会被忽略）
     * @return AddRecordResult 包含记录 ID 和是否是新记录
     */
    AddRecordResult addRecord(const ClipboardRecord& record);

    /**
     * 根据哈希查找记录
     *
     * @param hash SHA-256 哈希值
     * @return 记录，不存在返回空 optional
     */
    std::optional<ClipboardRecord> findByHash(const std::string& hash);

    /**
     * 根据 ID 获取记录
     *
     * @param id 记录 ID
     * @return 记录，不存在返回空 optional
     */
    std::optional<ClipboardRecord> getRecord(int64_t id);

    /**
     * 更新最后使用时间
     *
     * @param id 记录 ID
     * @return 是否成功
     */
    bool updateLastUsedTime(int64_t id);

    /**
     * 获取所有记录（按最后使用时间降序）
     *
     * @param limit 最大返回数量
     * @param offset 偏移量（用于分页）
     * @return 记录列表
     */
    std::vector<ClipboardRecord> getAllRecords(int limit = 100, int offset = 0);

    /**
     * 搜索文本记录（使用 FTS5）
     *
     * @param keyword 搜索关键词
     * @param limit 最大返回数量
     * @return 匹配的记录列表
     */
    std::vector<ClipboardRecord> searchText(const std::string& keyword, int limit = 100);

    /**
     * 删除记录
     *
     * @param id 记录 ID
     * @return 是否成功
     */
    bool deleteRecord(int64_t id);

    /**
     * 删除过期记录
     *
     * 根据时长限制和条数限制删除记录（取交集）。
     *
     * @param maxAgeDays 最大保留天数（0 表示不限制）
     * @param maxCount 最大保留条数（0 表示不限制）
     * @return 被删除的记录列表（用于清理关联的图片文件）
     */
    std::vector<ClipboardRecord> deleteExpiredRecords(int maxAgeDays, int maxCount);

    /**
     * 清空所有记录
     *
     * @return 被删除的记录列表（用于清理关联的图片文件）
     */
    std::vector<ClipboardRecord> clearAll();

    /**
     * 获取记录总数
     */
    int64_t getRecordCount();

private:
    ClipboardStore() = default;
    ~ClipboardStore();

    // 数据库操作
    bool openDatabase();
    void closeDatabase();
    bool createTables();
    bool prepareStatements();
    void finalizeStatements();

    // 内部辅助
    ClipboardRecord rowToRecord(sqlite3_stmt* stmt) const;
    int64_t getCurrentTimestampMs() const;
    std::vector<ClipboardRecord> searchTextFallback(const std::string& keyword, int limit);

    // 成员变量
    bool initialized_ = false;
    std::string dbPath_;
    sqlite3* db_ = nullptr;

    // 预编译语句
    sqlite3_stmt* stmtInsert_ = nullptr;
    sqlite3_stmt* stmtFindByHash_ = nullptr;
    sqlite3_stmt* stmtGetById_ = nullptr;
    sqlite3_stmt* stmtUpdateLastUsed_ = nullptr;
    sqlite3_stmt* stmtGetAll_ = nullptr;
    sqlite3_stmt* stmtDelete_ = nullptr;
    sqlite3_stmt* stmtCount_ = nullptr;
    sqlite3_stmt* stmtUpdateTimestamp_ = nullptr;
    sqlite3_stmt* stmtSearchFts_ = nullptr;      // FTS 搜索预编译语句
    sqlite3_stmt* stmtSearchLike_ = nullptr;     // LIKE 搜索预编译语句（降级方案）
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_CLIPBOARD_STORE_H
