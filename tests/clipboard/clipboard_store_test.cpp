/**
 * ClipboardStore 单元测试
 *
 * 测试剪贴板存储层的 SQLite 存储、CRUD 操作、FTS 搜索和过期清理功能。
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include "clipboard_store.h"

namespace fs = std::filesystem;

// 测试辅助宏
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "✗ 断言失败: " << message << std::endl; \
            std::cerr << "  位置: " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    std::cout << "✓ " << message << std::endl

class ClipboardStoreTest {
public:
    ClipboardStoreTest() {
        // 使用临时目录进行测试
        testDataDir_ = fs::temp_directory_path().string() + "/suyan_clipboard_test";
        testDbPath_ = testDataDir_ + "/clipboard.db";
        
        // 清理之前的测试数据
        fs::remove_all(testDataDir_);
    }
    
    ~ClipboardStoreTest() {
        // 关闭存储
        suyan::ClipboardStore::instance().shutdown();
        // 清理测试数据
        fs::remove_all(testDataDir_);
    }
    
    bool runAllTests() {
        std::cout << "=== ClipboardStore 单元测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testDataDir_ << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        // 基础测试
        allPassed &= testGetInstance();
        allPassed &= testInitialize();
        
        // CRUD 测试
        allPassed &= testAddTextRecord();
        allPassed &= testAddImageRecord();
        allPassed &= testHashDeduplication();
        allPassed &= testFindByHash();
        allPassed &= testGetRecord();
        allPassed &= testUpdateLastUsedTime();
        allPassed &= testGetAllRecords();
        allPassed &= testGetAllRecordsPagination();
        allPassed &= testDeleteRecord();
        
        // 搜索测试
        allPassed &= testSearchText();
        allPassed &= testSearchTextNoMatch();
        
        // 清理测试
        allPassed &= testDeleteExpiredByAge();
        allPassed &= testDeleteExpiredByCount();
        allPassed &= testDeleteExpiredByCombined();
        allPassed &= testClearAll();
        
        // 计数测试
        allPassed &= testGetRecordCount();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }
        
        return allPassed;
    }
    
private:
    std::string testDataDir_;
    std::string testDbPath_;
    
    // 重置测试环境
    void resetTestEnvironment() {
        auto& store = suyan::ClipboardStore::instance();
        if (store.isInitialized()) {
            store.clearAll();
        }
    }
    
    // 创建测试文本记录
    suyan::ClipboardRecord createTextRecord(const std::string& content, 
                                             const std::string& hash,
                                             const std::string& sourceApp = "com.test.app") {
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = content;
        record.contentHash = hash;
        record.sourceApp = sourceApp;
        return record;
    }
    
    // 创建测试图片记录
    suyan::ClipboardRecord createImageRecord(const std::string& path,
                                              const std::string& hash,
                                              const std::string& thumbnailPath = "",
                                              const std::string& format = "png") {
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Image;
        record.content = path;
        record.contentHash = hash;
        record.thumbnailPath = thumbnailPath;
        record.imageFormat = format;
        record.imageWidth = 800;
        record.imageHeight = 600;
        record.fileSize = 102400;
        record.sourceApp = "com.test.app";
        return record;
    }
    
    // ========== 基础测试 ==========
    
    bool testGetInstance() {
        auto& store1 = suyan::ClipboardStore::instance();
        auto& store2 = suyan::ClipboardStore::instance();
        TEST_ASSERT(&store1 == &store2, "单例实例应该相同");
        TEST_PASS("testGetInstance: 单例模式正常");
        return true;
    }
    
    bool testInitialize() {
        auto& store = suyan::ClipboardStore::instance();
        
        // 首次初始化
        bool result = store.initialize(testDbPath_);
        TEST_ASSERT(result, "初始化应该成功");
        TEST_ASSERT(store.isInitialized(), "初始化后 isInitialized 应该返回 true");
        
        // 检查数据目录已创建
        TEST_ASSERT(fs::exists(testDataDir_), "数据目录应该已创建");
        
        // 检查数据库文件已创建
        std::string dbPath = store.getDatabasePath();
        TEST_ASSERT(fs::exists(dbPath), "数据库文件应该已创建");
        std::cout << "  数据库路径: " << dbPath << std::endl;
        
        // 重复初始化应该返回 true（幂等）
        result = store.initialize(testDbPath_);
        TEST_ASSERT(result, "重复初始化应该返回 true");
        
        TEST_PASS("testInitialize: 初始化成功");
        return true;
    }
    
    // ========== CRUD 测试 ==========
    
    bool testAddTextRecord() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        auto record = createTextRecord("Hello, World!", "hash_text_001");
        int64_t id = store.addRecord(record);
        
        TEST_ASSERT(id > 0, "添加记录应该返回有效 ID");
        
        // 验证记录已添加
        auto retrieved = store.getRecord(id);
        TEST_ASSERT(retrieved.has_value(), "应该能获取到记录");
        TEST_ASSERT(retrieved->content == "Hello, World!", "内容应该正确");
        TEST_ASSERT(retrieved->type == suyan::ClipboardContentType::Text, "类型应该是文本");
        TEST_ASSERT(retrieved->contentHash == "hash_text_001", "哈希应该正确");
        TEST_ASSERT(retrieved->createdAt > 0, "创建时间应该大于 0");
        TEST_ASSERT(retrieved->lastUsedAt > 0, "最后使用时间应该大于 0");
        
        TEST_PASS("testAddTextRecord: 添加文本记录正常");
        return true;
    }
    
    bool testAddImageRecord() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        auto record = createImageRecord("/path/to/image.png", "hash_image_001", 
                                        "/path/to/thumb.png", "png");
        int64_t id = store.addRecord(record);
        
        TEST_ASSERT(id > 0, "添加记录应该返回有效 ID");
        
        // 验证记录已添加
        auto retrieved = store.getRecord(id);
        TEST_ASSERT(retrieved.has_value(), "应该能获取到记录");
        TEST_ASSERT(retrieved->content == "/path/to/image.png", "路径应该正确");
        TEST_ASSERT(retrieved->type == suyan::ClipboardContentType::Image, "类型应该是图片");
        TEST_ASSERT(retrieved->thumbnailPath == "/path/to/thumb.png", "缩略图路径应该正确");
        TEST_ASSERT(retrieved->imageFormat == "png", "格式应该正确");
        TEST_ASSERT(retrieved->imageWidth == 800, "宽度应该正确");
        TEST_ASSERT(retrieved->imageHeight == 600, "高度应该正确");
        TEST_ASSERT(retrieved->fileSize == 102400, "文件大小应该正确");
        
        TEST_PASS("testAddImageRecord: 添加图片记录正常");
        return true;
    }
    
    bool testHashDeduplication() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加第一条记录
        auto record1 = createTextRecord("Duplicate content", "hash_dup_001");
        int64_t id1 = store.addRecord(record1);
        TEST_ASSERT(id1 > 0, "第一条记录应该添加成功");
        
        // 获取第一条记录的时间戳
        auto retrieved1 = store.getRecord(id1);
        int64_t originalLastUsed = retrieved1->lastUsedAt;
        
        // 等待一小段时间确保时间戳不同
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 添加相同哈希的记录
        auto record2 = createTextRecord("Duplicate content", "hash_dup_001");
        int64_t id2 = store.addRecord(record2);
        
        // 应该返回相同的 ID
        TEST_ASSERT(id2 == id1, "重复哈希应该返回相同 ID");
        
        // 验证时间戳已更新
        auto retrieved2 = store.getRecord(id1);
        TEST_ASSERT(retrieved2->lastUsedAt >= originalLastUsed, "时间戳应该已更新");
        
        // 验证只有一条记录
        TEST_ASSERT(store.getRecordCount() == 1, "应该只有一条记录");
        
        TEST_PASS("testHashDeduplication: 哈希去重正常");
        return true;
    }
    
    bool testFindByHash() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 查找不存在的哈希
        auto notFound = store.findByHash("nonexistent_hash");
        TEST_ASSERT(!notFound.has_value(), "不存在的哈希应该返回空");
        
        // 添加记录
        auto record = createTextRecord("Find by hash test", "hash_find_001");
        store.addRecord(record);
        
        // 查找存在的哈希
        auto found = store.findByHash("hash_find_001");
        TEST_ASSERT(found.has_value(), "存在的哈希应该能找到");
        TEST_ASSERT(found->content == "Find by hash test", "内容应该正确");
        
        TEST_PASS("testFindByHash: 按哈希查找正常");
        return true;
    }
    
    bool testGetRecord() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 查找不存在的 ID
        auto notFound = store.getRecord(99999);
        TEST_ASSERT(!notFound.has_value(), "不存在的 ID 应该返回空");
        
        // 添加记录
        auto record = createTextRecord("Get record test", "hash_get_001");
        int64_t id = store.addRecord(record);
        
        // 查找存在的 ID
        auto found = store.getRecord(id);
        TEST_ASSERT(found.has_value(), "存在的 ID 应该能找到");
        TEST_ASSERT(found->id == id, "ID 应该正确");
        
        TEST_PASS("testGetRecord: 按 ID 获取记录正常");
        return true;
    }
    
    bool testUpdateLastUsedTime() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加记录
        auto record = createTextRecord("Update time test", "hash_update_001");
        int64_t id = store.addRecord(record);
        
        auto original = store.getRecord(id);
        int64_t originalTime = original->lastUsedAt;
        
        // 等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 更新时间
        bool result = store.updateLastUsedTime(id);
        TEST_ASSERT(result, "更新时间应该成功");
        
        // 验证时间已更新
        auto updated = store.getRecord(id);
        TEST_ASSERT(updated->lastUsedAt >= originalTime, "时间应该已更新");
        
        // 更新不存在的 ID
        result = store.updateLastUsedTime(99999);
        TEST_ASSERT(!result, "更新不存在的 ID 应该失败");
        
        TEST_PASS("testUpdateLastUsedTime: 更新最后使用时间正常");
        return true;
    }
    
    bool testGetAllRecords() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 空数据库
        auto empty = store.getAllRecords();
        TEST_ASSERT(empty.empty(), "空数据库应该返回空列表");
        
        // 添加多条记录
        store.addRecord(createTextRecord("Record 1", "hash_all_001"));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        store.addRecord(createTextRecord("Record 2", "hash_all_002"));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        store.addRecord(createTextRecord("Record 3", "hash_all_003"));
        
        // 获取所有记录
        auto all = store.getAllRecords();
        TEST_ASSERT(all.size() == 3, "应该返回 3 条记录");
        
        // 验证按最后使用时间降序排列
        TEST_ASSERT(all[0].content == "Record 3", "第一条应该是最新的");
        TEST_ASSERT(all[2].content == "Record 1", "最后一条应该是最旧的");
        
        TEST_PASS("testGetAllRecords: 获取所有记录正常");
        return true;
    }
    
    bool testGetAllRecordsPagination() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加 5 条记录
        for (int i = 1; i <= 5; i++) {
            store.addRecord(createTextRecord("Record " + std::to_string(i), 
                                             "hash_page_" + std::to_string(i)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        // 测试分页
        auto page1 = store.getAllRecords(2, 0);
        TEST_ASSERT(page1.size() == 2, "第一页应该有 2 条记录");
        TEST_ASSERT(page1[0].content == "Record 5", "第一页第一条应该是最新的");
        
        auto page2 = store.getAllRecords(2, 2);
        TEST_ASSERT(page2.size() == 2, "第二页应该有 2 条记录");
        TEST_ASSERT(page2[0].content == "Record 3", "第二页第一条应该正确");
        
        auto page3 = store.getAllRecords(2, 4);
        TEST_ASSERT(page3.size() == 1, "第三页应该有 1 条记录");
        
        TEST_PASS("testGetAllRecordsPagination: 分页获取记录正常");
        return true;
    }
    
    bool testDeleteRecord() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加记录
        auto record = createTextRecord("Delete test", "hash_delete_001");
        int64_t id = store.addRecord(record);
        
        // 验证记录存在
        TEST_ASSERT(store.getRecord(id).has_value(), "记录应该存在");
        
        // 删除记录
        bool result = store.deleteRecord(id);
        TEST_ASSERT(result, "删除应该成功");
        
        // 验证记录已删除
        TEST_ASSERT(!store.getRecord(id).has_value(), "记录应该已删除");
        
        // 删除不存在的记录
        result = store.deleteRecord(99999);
        TEST_ASSERT(!result, "删除不存在的记录应该失败");
        
        TEST_PASS("testDeleteRecord: 删除记录正常");
        return true;
    }
    
    // ========== 搜索测试 ==========
    
    bool testSearchText() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加测试数据
        store.addRecord(createTextRecord("Hello World", "hash_search_001"));
        store.addRecord(createTextRecord("Hello China", "hash_search_002"));
        store.addRecord(createTextRecord("Goodbye World", "hash_search_003"));
        store.addRecord(createImageRecord("/path/image.png", "hash_search_004")); // 图片不参与搜索
        
        // 搜索 "Hello"
        auto results = store.searchText("Hello");
        TEST_ASSERT(results.size() == 2, "搜索 Hello 应该返回 2 条结果");
        
        // 搜索 "World"
        results = store.searchText("World");
        TEST_ASSERT(results.size() == 2, "搜索 World 应该返回 2 条结果");
        
        // 搜索 "China"
        results = store.searchText("China");
        TEST_ASSERT(results.size() == 1, "搜索 China 应该返回 1 条结果");
        TEST_ASSERT(results[0].content == "Hello China", "内容应该正确");
        
        TEST_PASS("testSearchText: 文本搜索正常");
        return true;
    }
    
    bool testSearchTextNoMatch() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加测试数据
        store.addRecord(createTextRecord("Hello World", "hash_nomatch_001"));
        
        // 搜索不存在的关键词
        auto results = store.searchText("NotExist");
        TEST_ASSERT(results.empty(), "搜索不存在的关键词应该返回空");
        
        // 空关键词
        results = store.searchText("");
        TEST_ASSERT(results.empty(), "空关键词应该返回空");
        
        TEST_PASS("testSearchTextNoMatch: 无匹配搜索正常");
        return true;
    }
    
    // ========== 清理测试 ==========
    
    bool testDeleteExpiredByAge() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加记录
        store.addRecord(createTextRecord("Recent", "hash_age_001"));
        
        // 删除超过 1 天的记录（刚添加的不会被删除）
        auto deleted = store.deleteExpiredRecords(1, 0);
        TEST_ASSERT(deleted.empty(), "刚添加的记录不应该被删除");
        TEST_ASSERT(store.getRecordCount() == 1, "应该还有 1 条记录");
        
        TEST_PASS("testDeleteExpiredByAge: 按时间清理正常");
        return true;
    }
    
    bool testDeleteExpiredByCount() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加 5 条记录
        for (int i = 1; i <= 5; i++) {
            store.addRecord(createTextRecord("Record " + std::to_string(i), 
                                             "hash_count_" + std::to_string(i)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        TEST_ASSERT(store.getRecordCount() == 5, "应该有 5 条记录");
        
        // 保留最近 3 条
        auto deleted = store.deleteExpiredRecords(0, 3);
        TEST_ASSERT(deleted.size() == 2, "应该删除 2 条记录");
        TEST_ASSERT(store.getRecordCount() == 3, "应该剩余 3 条记录");
        
        // 验证保留的是最新的 3 条
        auto remaining = store.getAllRecords();
        TEST_ASSERT(remaining[0].content == "Record 5", "最新的应该保留");
        TEST_ASSERT(remaining[2].content == "Record 3", "第三新的应该保留");
        
        TEST_PASS("testDeleteExpiredByCount: 按条数清理正常");
        return true;
    }
    
    bool testDeleteExpiredByCombined() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加记录
        for (int i = 1; i <= 3; i++) {
            store.addRecord(createTextRecord("Record " + std::to_string(i), 
                                             "hash_combined_" + std::to_string(i)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        // 同时设置时间和条数限制
        // 由于记录都是刚添加的，时间限制不会生效
        // 根据需求 3.3：只有同时超过时长限制和条数限制的记录才会被删除
        // 所以刚添加的记录不会被删除（不满足时间限制条件）
        auto deleted = store.deleteExpiredRecords(30, 2);
        
        // 由于记录都是新的，不满足时间限制，所以不会删除任何记录
        TEST_ASSERT(deleted.empty(), "新记录不应该被删除（不满足时间限制）");
        TEST_ASSERT(store.getRecordCount() == 3, "应该剩余 3 条记录");
        
        TEST_PASS("testDeleteExpiredByCombined: 组合清理正常（AND 逻辑）");
        return true;
    }
    
    bool testClearAll() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 添加记录
        store.addRecord(createTextRecord("Clear 1", "hash_clear_001"));
        store.addRecord(createTextRecord("Clear 2", "hash_clear_002"));
        store.addRecord(createImageRecord("/path/img.png", "hash_clear_003"));
        
        TEST_ASSERT(store.getRecordCount() == 3, "应该有 3 条记录");
        
        // 清空
        auto deleted = store.clearAll();
        TEST_ASSERT(deleted.size() == 3, "应该返回 3 条被删除的记录");
        TEST_ASSERT(store.getRecordCount() == 0, "清空后应该没有记录");
        
        // 验证返回的记录包含图片记录（用于清理文件）
        bool hasImage = false;
        for (const auto& r : deleted) {
            if (r.type == suyan::ClipboardContentType::Image) {
                hasImage = true;
                break;
            }
        }
        TEST_ASSERT(hasImage, "返回的记录应该包含图片记录");
        
        TEST_PASS("testClearAll: 清空所有记录正常");
        return true;
    }
    
    // ========== 计数测试 ==========
    
    bool testGetRecordCount() {
        resetTestEnvironment();
        auto& store = suyan::ClipboardStore::instance();
        
        // 初始应该是 0
        TEST_ASSERT(store.getRecordCount() == 0, "初始记录数应该是 0");
        
        // 添加记录
        store.addRecord(createTextRecord("Count 1", "hash_count_a"));
        TEST_ASSERT(store.getRecordCount() == 1, "添加后应该是 1");
        
        store.addRecord(createTextRecord("Count 2", "hash_count_b"));
        TEST_ASSERT(store.getRecordCount() == 2, "添加后应该是 2");
        
        // 添加重复哈希不增加计数
        store.addRecord(createTextRecord("Count 1 dup", "hash_count_a"));
        TEST_ASSERT(store.getRecordCount() == 2, "重复哈希不应该增加计数");
        
        TEST_PASS("testGetRecordCount: 获取记录数正常");
        return true;
    }
};

int main(int argc, char* argv[]) {
    // 需要 QCoreApplication 来支持某些 Qt 功能
    QCoreApplication app(argc, argv);
    
    ClipboardStoreTest test;
    return test.runAllTests() ? 0 : 1;
}
