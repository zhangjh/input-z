/**
 * ClipboardStore 实现
 *
 * 使用 SQLite 进行剪贴板历史记录的持久化存储。
 * 支持 FTS5 全文搜索。
 */

#include "clipboard_store.h"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace suyan {

// ========== 单例实现 ==========

ClipboardStore& ClipboardStore::instance() {
    static ClipboardStore instance;
    return instance;
}

ClipboardStore::~ClipboardStore() {
    shutdown();
}

// ========== 初始化和关闭 ==========

bool ClipboardStore::initialize(const std::string& dbPath) {
    if (initialized_) {
        // 如果已初始化且路径相同，直接返回成功
        if (dbPath_ == dbPath) {
            return true;
        }
        // 路径不同，先关闭
        shutdown();
    }

    dbPath_ = dbPath;

    // 确保目录存在
    try {
        fs::path dbDir = fs::path(dbPath).parent_path();
        if (!dbDir.empty()) {
            fs::create_directories(dbDir);
        }
    } catch (const std::exception& e) {
        std::cerr << "ClipboardStore: 创建目录失败: " << e.what() << std::endl;
        return false;
    }

    // 打开数据库
    if (!openDatabase()) {
        return false;
    }

    // 创建表
    if (!createTables()) {
        closeDatabase();
        return false;
    }

    // 准备预编译语句
    if (!prepareStatements()) {
        closeDatabase();
        return false;
    }

    initialized_ = true;
    return true;
}

void ClipboardStore::shutdown() {
    if (!initialized_) {
        return;
    }

    finalizeStatements();
    closeDatabase();
    initialized_ = false;
}

// ========== 数据库操作 ==========

bool ClipboardStore::openDatabase() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 打开数据库失败: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // 启用 WAL 模式提高性能
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 设置 WAL 模式失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // 不是致命错误，继续
    }

    // 启用外键约束
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    return true;
}

void ClipboardStore::closeDatabase() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool ClipboardStore::createTables() {
    // 创建主表
    const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS clipboard_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content_type INTEGER NOT NULL,
            content TEXT NOT NULL,
            content_hash TEXT NOT NULL UNIQUE,
            source_app TEXT,
            thumbnail_path TEXT,
            image_format TEXT,
            image_width INTEGER DEFAULT 0,
            image_height INTEGER DEFAULT 0,
            file_size INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            last_used_at INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_clipboard_hash 
            ON clipboard_history(content_hash);
        CREATE INDEX IF NOT EXISTS idx_clipboard_last_used 
            ON clipboard_history(last_used_at DESC);
        CREATE INDEX IF NOT EXISTS idx_clipboard_created 
            ON clipboard_history(created_at DESC);
        CREATE INDEX IF NOT EXISTS idx_clipboard_type 
            ON clipboard_history(content_type);
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 创建主表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    // 创建 FTS5 虚拟表（仅用于文本搜索）
    const char* createFtsSQL = R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS clipboard_fts USING fts5(
            content,
            content='clipboard_history',
            content_rowid='id'
        );
    )";

    rc = sqlite3_exec(db_, createFtsSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 创建 FTS 表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // FTS 创建失败不是致命错误，搜索功能会降级
    }

    // 创建触发器同步 FTS 索引
    const char* createTriggersSQL = R"(
        -- 插入触发器：仅对文本类型创建 FTS 索引
        CREATE TRIGGER IF NOT EXISTS clipboard_ai AFTER INSERT ON clipboard_history
        WHEN NEW.content_type = 0
        BEGIN
            INSERT INTO clipboard_fts(rowid, content) VALUES (NEW.id, NEW.content);
        END;

        -- 删除触发器
        CREATE TRIGGER IF NOT EXISTS clipboard_ad AFTER DELETE ON clipboard_history
        WHEN OLD.content_type = 0
        BEGIN
            INSERT INTO clipboard_fts(clipboard_fts, rowid, content) 
            VALUES ('delete', OLD.id, OLD.content);
        END;

        -- 更新触发器
        CREATE TRIGGER IF NOT EXISTS clipboard_au AFTER UPDATE ON clipboard_history
        WHEN OLD.content_type = 0
        BEGIN
            INSERT INTO clipboard_fts(clipboard_fts, rowid, content) 
            VALUES ('delete', OLD.id, OLD.content);
            INSERT INTO clipboard_fts(rowid, content) VALUES (NEW.id, NEW.content);
        END;
    )";

    rc = sqlite3_exec(db_, createTriggersSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 创建触发器失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // 触发器创建失败不是致命错误
    }

    return true;
}

bool ClipboardStore::prepareStatements() {
    int rc;

    // INSERT 语句
    const char* insertSQL = R"(
        INSERT INTO clipboard_history 
        (content_type, content, content_hash, source_app, thumbnail_path, 
         image_format, image_width, image_height, file_size, created_at, last_used_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    rc = sqlite3_prepare_v2(db_, insertSQL, -1, &stmtInsert_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 INSERT 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // FIND BY HASH 语句
    const char* findByHashSQL = R"(
        SELECT id, content_type, content, content_hash, source_app, thumbnail_path,
               image_format, image_width, image_height, file_size, created_at, last_used_at
        FROM clipboard_history
        WHERE content_hash = ?
    )";
    rc = sqlite3_prepare_v2(db_, findByHashSQL, -1, &stmtFindByHash_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 FIND BY HASH 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // GET BY ID 语句
    const char* getByIdSQL = R"(
        SELECT id, content_type, content, content_hash, source_app, thumbnail_path,
               image_format, image_width, image_height, file_size, created_at, last_used_at
        FROM clipboard_history
        WHERE id = ?
    )";
    rc = sqlite3_prepare_v2(db_, getByIdSQL, -1, &stmtGetById_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 GET BY ID 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // UPDATE LAST USED 语句
    const char* updateLastUsedSQL = R"(
        UPDATE clipboard_history SET last_used_at = ? WHERE id = ?
    )";
    rc = sqlite3_prepare_v2(db_, updateLastUsedSQL, -1, &stmtUpdateLastUsed_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 UPDATE LAST USED 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // GET ALL 语句
    const char* getAllSQL = R"(
        SELECT id, content_type, content, content_hash, source_app, thumbnail_path,
               image_format, image_width, image_height, file_size, created_at, last_used_at
        FROM clipboard_history
        ORDER BY last_used_at DESC
        LIMIT ? OFFSET ?
    )";
    rc = sqlite3_prepare_v2(db_, getAllSQL, -1, &stmtGetAll_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 GET ALL 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // DELETE 语句
    const char* deleteSQL = R"(
        DELETE FROM clipboard_history WHERE id = ?
    )";
    rc = sqlite3_prepare_v2(db_, deleteSQL, -1, &stmtDelete_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 DELETE 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // COUNT 语句
    const char* countSQL = R"(
        SELECT COUNT(*) FROM clipboard_history
    )";
    rc = sqlite3_prepare_v2(db_, countSQL, -1, &stmtCount_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 COUNT 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // UPDATE TIMESTAMP（用于哈希去重时更新时间戳）
    const char* updateTimestampSQL = R"(
        UPDATE clipboard_history SET last_used_at = ? WHERE content_hash = ?
    )";
    rc = sqlite3_prepare_v2(db_, updateTimestampSQL, -1, &stmtUpdateTimestamp_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 UPDATE TIMESTAMP 语句失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // FTS 搜索语句（预编译以提高性能）
    const char* searchFtsSQL = R"(
        SELECT h.id, h.content_type, h.content, h.content_hash, h.source_app, 
               h.thumbnail_path, h.image_format, h.image_width, h.image_height, 
               h.file_size, h.created_at, h.last_used_at
        FROM clipboard_history h
        INNER JOIN clipboard_fts f ON h.id = f.rowid
        WHERE clipboard_fts MATCH ?
        ORDER BY h.last_used_at DESC
        LIMIT ?
    )";
    rc = sqlite3_prepare_v2(db_, searchFtsSQL, -1, &stmtSearchFts_, nullptr);
    if (rc != SQLITE_OK) {
        // FTS 搜索语句准备失败不是致命错误，会降级到 LIKE 搜索
        std::cerr << "ClipboardStore: 准备 FTS 搜索语句失败（将使用 LIKE 降级）: " << sqlite3_errmsg(db_) << std::endl;
        stmtSearchFts_ = nullptr;
    }

    // LIKE 搜索语句（降级方案，预编译以提高性能）
    const char* searchLikeSQL = R"(
        SELECT id, content_type, content, content_hash, source_app, 
               thumbnail_path, image_format, image_width, image_height, 
               file_size, created_at, last_used_at
        FROM clipboard_history
        WHERE content_type = 0 AND content LIKE ?
        ORDER BY last_used_at DESC
        LIMIT ?
    )";
    rc = sqlite3_prepare_v2(db_, searchLikeSQL, -1, &stmtSearchLike_, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 准备 LIKE 搜索语句失败: " << sqlite3_errmsg(db_) << std::endl;
        // LIKE 搜索失败也不是致命错误
        stmtSearchLike_ = nullptr;
    }

    return true;
}

void ClipboardStore::finalizeStatements() {
    if (stmtInsert_) { sqlite3_finalize(stmtInsert_); stmtInsert_ = nullptr; }
    if (stmtFindByHash_) { sqlite3_finalize(stmtFindByHash_); stmtFindByHash_ = nullptr; }
    if (stmtGetById_) { sqlite3_finalize(stmtGetById_); stmtGetById_ = nullptr; }
    if (stmtUpdateLastUsed_) { sqlite3_finalize(stmtUpdateLastUsed_); stmtUpdateLastUsed_ = nullptr; }
    if (stmtGetAll_) { sqlite3_finalize(stmtGetAll_); stmtGetAll_ = nullptr; }
    if (stmtDelete_) { sqlite3_finalize(stmtDelete_); stmtDelete_ = nullptr; }
    if (stmtCount_) { sqlite3_finalize(stmtCount_); stmtCount_ = nullptr; }
    if (stmtUpdateTimestamp_) { sqlite3_finalize(stmtUpdateTimestamp_); stmtUpdateTimestamp_ = nullptr; }
    if (stmtSearchFts_) { sqlite3_finalize(stmtSearchFts_); stmtSearchFts_ = nullptr; }
    if (stmtSearchLike_) { sqlite3_finalize(stmtSearchLike_); stmtSearchLike_ = nullptr; }
}

// ========== 辅助方法 ==========

ClipboardRecord ClipboardStore::rowToRecord(sqlite3_stmt* stmt) const {
    ClipboardRecord record;
    record.id = sqlite3_column_int64(stmt, 0);
    record.type = static_cast<ClipboardContentType>(sqlite3_column_int(stmt, 1));
    
    const char* content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    record.content = content ? content : "";
    
    const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    record.contentHash = hash ? hash : "";
    
    const char* sourceApp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    record.sourceApp = sourceApp ? sourceApp : "";
    
    const char* thumbnailPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    record.thumbnailPath = thumbnailPath ? thumbnailPath : "";
    
    const char* imageFormat = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    record.imageFormat = imageFormat ? imageFormat : "";
    
    record.imageWidth = sqlite3_column_int(stmt, 7);
    record.imageHeight = sqlite3_column_int(stmt, 8);
    record.fileSize = sqlite3_column_int64(stmt, 9);
    record.createdAt = sqlite3_column_int64(stmt, 10);
    record.lastUsedAt = sqlite3_column_int64(stmt, 11);
    
    return record;
}

int64_t ClipboardStore::getCurrentTimestampMs() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    );
    return ms.count();
}

// ========== CRUD 操作 ==========

AddRecordResult ClipboardStore::addRecord(const ClipboardRecord& record) {
    AddRecordResult result;
    
    if (!initialized_) {
        return result;
    }

    // 先检查是否已存在相同哈希的记录
    auto existing = findByHash(record.contentHash);
    if (existing) {
        // 更新时间戳并返回已有 ID（不是新记录）
        int64_t now = getCurrentTimestampMs();
        sqlite3_reset(stmtUpdateTimestamp_);
        sqlite3_bind_int64(stmtUpdateTimestamp_, 1, now);
        sqlite3_bind_text(stmtUpdateTimestamp_, 2, record.contentHash.c_str(), -1, SQLITE_TRANSIENT);
        
        int rc = sqlite3_step(stmtUpdateTimestamp_);
        if (rc != SQLITE_DONE) {
            std::cerr << "ClipboardStore: 更新时间戳失败: " << sqlite3_errmsg(db_) << std::endl;
        }
        result.id = existing->id;
        result.isNew = false;
        return result;
    }

    // 插入新记录
    int64_t now = getCurrentTimestampMs();
    
    sqlite3_reset(stmtInsert_);
    sqlite3_bind_int(stmtInsert_, 1, static_cast<int>(record.type));
    sqlite3_bind_text(stmtInsert_, 2, record.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtInsert_, 3, record.contentHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtInsert_, 4, record.sourceApp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtInsert_, 5, record.thumbnailPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmtInsert_, 6, record.imageFormat.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmtInsert_, 7, record.imageWidth);
    sqlite3_bind_int(stmtInsert_, 8, record.imageHeight);
    sqlite3_bind_int64(stmtInsert_, 9, record.fileSize);
    sqlite3_bind_int64(stmtInsert_, 10, now);
    sqlite3_bind_int64(stmtInsert_, 11, now);

    int rc = sqlite3_step(stmtInsert_);
    if (rc != SQLITE_DONE) {
        std::cerr << "ClipboardStore: 插入记录失败: " << sqlite3_errmsg(db_) << std::endl;
        return result;
    }

    result.id = sqlite3_last_insert_rowid(db_);
    result.isNew = true;
    return result;
}

std::optional<ClipboardRecord> ClipboardStore::findByHash(const std::string& hash) {
    if (!initialized_ || hash.empty()) {
        return std::nullopt;
    }

    sqlite3_reset(stmtFindByHash_);
    sqlite3_bind_text(stmtFindByHash_, 1, hash.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmtFindByHash_);
    if (rc == SQLITE_ROW) {
        return rowToRecord(stmtFindByHash_);
    }

    return std::nullopt;
}

std::optional<ClipboardRecord> ClipboardStore::getRecord(int64_t id) {
    if (!initialized_ || id <= 0) {
        return std::nullopt;
    }

    sqlite3_reset(stmtGetById_);
    sqlite3_bind_int64(stmtGetById_, 1, id);

    int rc = sqlite3_step(stmtGetById_);
    if (rc == SQLITE_ROW) {
        return rowToRecord(stmtGetById_);
    }

    return std::nullopt;
}

bool ClipboardStore::updateLastUsedTime(int64_t id) {
    if (!initialized_ || id <= 0) {
        return false;
    }

    int64_t now = getCurrentTimestampMs();
    
    sqlite3_reset(stmtUpdateLastUsed_);
    sqlite3_bind_int64(stmtUpdateLastUsed_, 1, now);
    sqlite3_bind_int64(stmtUpdateLastUsed_, 2, id);

    int rc = sqlite3_step(stmtUpdateLastUsed_);
    if (rc != SQLITE_DONE) {
        std::cerr << "ClipboardStore: 更新最后使用时间失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

std::vector<ClipboardRecord> ClipboardStore::getAllRecords(int limit, int offset) {
    std::vector<ClipboardRecord> results;
    
    if (!initialized_) {
        return results;
    }

    sqlite3_reset(stmtGetAll_);
    sqlite3_bind_int(stmtGetAll_, 1, limit);
    sqlite3_bind_int(stmtGetAll_, 2, offset);

    while (sqlite3_step(stmtGetAll_) == SQLITE_ROW) {
        results.push_back(rowToRecord(stmtGetAll_));
    }

    return results;
}

std::vector<ClipboardRecord> ClipboardStore::searchText(const std::string& keyword, int limit) {
    std::vector<ClipboardRecord> results;
    
    if (!initialized_ || keyword.empty()) {
        return results;
    }

    // 优先使用预编译的 FTS 搜索语句
    if (stmtSearchFts_) {
        // 对关键词进行转义和处理
        // 添加通配符支持模糊匹配
        std::string searchQuery = "\"" + keyword + "\"*";

        sqlite3_reset(stmtSearchFts_);
        sqlite3_bind_text(stmtSearchFts_, 1, searchQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmtSearchFts_, 2, limit);

        while (sqlite3_step(stmtSearchFts_) == SQLITE_ROW) {
            results.push_back(rowToRecord(stmtSearchFts_));
        }

        // 如果 FTS 有结果，直接返回
        if (!results.empty()) {
            return results;
        }
    }

    // FTS 没有结果或不可用，降级到 LIKE 搜索
    return searchTextFallback(keyword, limit);
}

// 私有方法：LIKE 搜索降级
std::vector<ClipboardRecord> ClipboardStore::searchTextFallback(const std::string& keyword, int limit) {
    std::vector<ClipboardRecord> results;
    
    std::string likePattern = "%" + keyword + "%";

    // 优先使用预编译语句
    if (stmtSearchLike_) {
        sqlite3_reset(stmtSearchLike_);
        sqlite3_bind_text(stmtSearchLike_, 1, likePattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmtSearchLike_, 2, limit);

        while (sqlite3_step(stmtSearchLike_) == SQLITE_ROW) {
            results.push_back(rowToRecord(stmtSearchLike_));
        }
        return results;
    }

    // 预编译语句不可用，使用临时语句
    std::string sql = R"(
        SELECT id, content_type, content, content_hash, source_app, 
               thumbnail_path, image_format, image_width, image_height, 
               file_size, created_at, last_used_at
        FROM clipboard_history
        WHERE content_type = 0 AND content LIKE ?
        ORDER BY last_used_at DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: LIKE 搜索准备失败: " << sqlite3_errmsg(db_) << std::endl;
        return results;
    }

    sqlite3_bind_text(stmt, 1, likePattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToRecord(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool ClipboardStore::deleteRecord(int64_t id) {
    if (!initialized_ || id <= 0) {
        return false;
    }

    sqlite3_reset(stmtDelete_);
    sqlite3_bind_int64(stmtDelete_, 1, id);

    int rc = sqlite3_step(stmtDelete_);
    if (rc != SQLITE_DONE) {
        std::cerr << "ClipboardStore: 删除记录失败: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

std::vector<ClipboardRecord> ClipboardStore::deleteExpiredRecords(int maxAgeDays, int maxCount) {
    std::vector<ClipboardRecord> deletedRecords;
    
    if (!initialized_) {
        return deletedRecords;
    }

    // 如果两个限制都为 0，不删除任何记录
    if (maxAgeDays <= 0 && maxCount <= 0) {
        return deletedRecords;
    }

    // 计算时间阈值（毫秒）
    int64_t ageThreshold = 0;
    if (maxAgeDays > 0) {
        int64_t now = getCurrentTimestampMs();
        ageThreshold = now - (static_cast<int64_t>(maxAgeDays) * 24 * 60 * 60 * 1000);
    }

    // 构建查询条件
    // 策略：删除同时满足以下条件的记录：
    // 1. 超过时间限制（如果设置了）
    // 2. 超过条数限制（如果设置了）
    
    std::string sql;
    
    if (maxAgeDays > 0 && maxCount > 0) {
        // 两个条件都设置：删除超过时间限制 AND 超过条数限制的记录
        // 先获取需要保留的记录 ID（最近的 maxCount 条）
        sql = R"(
            SELECT id, content_type, content, content_hash, source_app, 
                   thumbnail_path, image_format, image_width, image_height, 
                   file_size, created_at, last_used_at
            FROM clipboard_history
            WHERE created_at < ?
              AND id NOT IN (
                  SELECT id FROM clipboard_history 
                  ORDER BY last_used_at DESC 
                  LIMIT ?
              )
        )";
    } else if (maxAgeDays > 0) {
        // 只有时间限制
        sql = R"(
            SELECT id, content_type, content, content_hash, source_app, 
                   thumbnail_path, image_format, image_width, image_height, 
                   file_size, created_at, last_used_at
            FROM clipboard_history
            WHERE created_at < ?
        )";
    } else {
        // 只有条数限制
        sql = R"(
            SELECT id, content_type, content, content_hash, source_app, 
                   thumbnail_path, image_format, image_width, image_height, 
                   file_size, created_at, last_used_at
            FROM clipboard_history
            WHERE id NOT IN (
                SELECT id FROM clipboard_history 
                ORDER BY last_used_at DESC 
                LIMIT ?
            )
        )";
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 查询过期记录失败: " << sqlite3_errmsg(db_) << std::endl;
        return deletedRecords;
    }

    if (maxAgeDays > 0 && maxCount > 0) {
        sqlite3_bind_int64(stmt, 1, ageThreshold);
        sqlite3_bind_int(stmt, 2, maxCount);
    } else if (maxAgeDays > 0) {
        sqlite3_bind_int64(stmt, 1, ageThreshold);
    } else {
        sqlite3_bind_int(stmt, 1, maxCount);
    }

    // 收集要删除的记录
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        deletedRecords.push_back(rowToRecord(stmt));
    }
    sqlite3_finalize(stmt);

    // 删除记录
    if (!deletedRecords.empty()) {
        char* errMsg = nullptr;
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);

        for (const auto& record : deletedRecords) {
            deleteRecord(record.id);
        }

        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg);
    }

    return deletedRecords;
}

std::vector<ClipboardRecord> ClipboardStore::clearAll() {
    std::vector<ClipboardRecord> deletedRecords;
    
    if (!initialized_) {
        return deletedRecords;
    }

    // 先获取所有记录（用于返回，以便清理图片文件）
    deletedRecords = getAllRecords(INT_MAX, 0);

    // 清空表
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM clipboard_history;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "ClipboardStore: 清空记录失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return {};
    }

    // 清空 FTS 索引
    sqlite3_exec(db_, "DELETE FROM clipboard_fts;", nullptr, nullptr, nullptr);

    return deletedRecords;
}

int64_t ClipboardStore::getRecordCount() {
    if (!initialized_) {
        return 0;
    }

    sqlite3_reset(stmtCount_);
    
    int rc = sqlite3_step(stmtCount_);
    if (rc == SQLITE_ROW) {
        return sqlite3_column_int64(stmtCount_, 0);
    }

    return 0;
}

} // namespace suyan
