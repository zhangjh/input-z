/**
 * ClipboardManager 单元测试
 *
 * 测试剪贴板管理器的核心功能：
 * - 初始化和关闭
 * - 监听控制
 * - 历史记录管理
 * - 粘贴操作
 * - 清理功能
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <QSignalSpy>
#include "clipboard_manager.h"
#include "clipboard_store.h"
#include "image_storage.h"

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

class ClipboardManagerTest {
public:
    ClipboardManagerTest() {
        // 使用临时目录进行测试
        testDataDir_ = fs::temp_directory_path().string() + "/suyan_clipboard_manager_test";
        
        // 清理之前的测试数据
        fs::remove_all(testDataDir_);
    }
    
    ~ClipboardManagerTest() {
        // 关闭管理器
        suyan::ClipboardManager::instance().shutdown();
        // 清理测试数据
        fs::remove_all(testDataDir_);
    }
    
    bool runAllTests() {
        std::cout << "=== ClipboardManager 单元测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testDataDir_ << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        // 基础测试
        allPassed &= testGetInstance();
        allPassed &= testInitialize();
        allPassed &= testInitializeIdempotent();
        
        // 监听控制测试
        allPassed &= testStartStopMonitoring();
        allPassed &= testMonitoringStateSignal();
        
        // 配置测试
        allPassed &= testSetEnabled();
        allPassed &= testSetMaxAgeDays();
        allPassed &= testSetMaxCount();
        
        // 历史记录管理测试
        allPassed &= testGetHistoryEmpty();
        allPassed &= testGetHistoryWithRecords();
        allPassed &= testSearchRecords();
        allPassed &= testDeleteRecord();
        allPassed &= testClearHistory();
        
        // 粘贴测试
        allPassed &= testPasteTextRecord();
        allPassed &= testPasteNonexistentRecord();
        
        // 清理测试
        allPassed &= testPerformCleanup();
        allPassed &= testPerformCleanupWithImages();
        
        // 信号测试
        allPassed &= testRecordDeletedSignal();
        allPassed &= testHistoryClearedSignal();
        
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
    
    // 重置测试环境
    void resetTestEnvironment() {
        auto& manager = suyan::ClipboardManager::instance();
        if (manager.isInitialized()) {
            manager.stopMonitoring();
            manager.clearHistory();
        }
    }
    
    // 确保管理器已初始化
    bool ensureInitialized() {
        auto& manager = suyan::ClipboardManager::instance();
        if (!manager.isInitialized()) {
            return manager.initialize(testDataDir_);
        }
        return true;
    }
    
    // 添加测试文本记录
    int64_t addTestTextRecord(const std::string& content, const std::string& hash) {
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = content;
        record.contentHash = hash;
        record.sourceApp = "com.test.app";
        return suyan::ClipboardStore::instance().addRecord(record);
    }
    
    // ========== 基础测试 ==========
    
    bool testGetInstance() {
        auto& manager1 = suyan::ClipboardManager::instance();
        auto& manager2 = suyan::ClipboardManager::instance();
        TEST_ASSERT(&manager1 == &manager2, "单例实例应该相同");
        TEST_PASS("testGetInstance: 单例模式正常");
        return true;
    }
    
    bool testInitialize() {
        auto& manager = suyan::ClipboardManager::instance();
        
        // 确保先关闭
        manager.shutdown();
        TEST_ASSERT(!manager.isInitialized(), "关闭后应该未初始化");
        
        // 初始化
        bool result = manager.initialize(testDataDir_);
        TEST_ASSERT(result, "初始化应该成功");
        TEST_ASSERT(manager.isInitialized(), "初始化后应该已初始化");
        TEST_ASSERT(manager.getDataDir() == testDataDir_, "数据目录应该正确");
        
        // 检查目录已创建
        TEST_ASSERT(fs::exists(testDataDir_), "数据目录应该已创建");
        TEST_ASSERT(fs::exists(testDataDir_ + "/clipboard"), "剪贴板目录应该已创建");
        
        TEST_PASS("testInitialize: 初始化成功");
        return true;
    }
    
    bool testInitializeIdempotent() {
        auto& manager = suyan::ClipboardManager::instance();
        
        // 确保已初始化
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        // 重复初始化应该返回 true
        bool result = manager.initialize(testDataDir_);
        TEST_ASSERT(result, "重复初始化应该返回 true");
        TEST_ASSERT(manager.isInitialized(), "应该仍然已初始化");
        
        TEST_PASS("testInitializeIdempotent: 幂等初始化正常");
        return true;
    }
    
    // ========== 监听控制测试 ==========
    
    bool testStartStopMonitoring() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        manager.setEnabled(true);
        
        // 启动监听
        bool result = manager.startMonitoring();
        TEST_ASSERT(result, "启动监听应该成功");
        TEST_ASSERT(manager.isMonitoring(), "应该正在监听");
        
        // 重复启动应该返回 true
        result = manager.startMonitoring();
        TEST_ASSERT(result, "重复启动应该返回 true");
        
        // 停止监听
        manager.stopMonitoring();
        TEST_ASSERT(!manager.isMonitoring(), "应该已停止监听");
        
        // 重复停止不应该出错
        manager.stopMonitoring();
        TEST_ASSERT(!manager.isMonitoring(), "应该仍然未监听");
        
        TEST_PASS("testStartStopMonitoring: 监听控制正常");
        return true;
    }
    
    bool testMonitoringStateSignal() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        manager.setEnabled(true);
        manager.stopMonitoring();
        
        // 监听信号
        QSignalSpy spy(&manager, &suyan::ClipboardManager::monitoringStateChanged);
        
        // 启动监听
        manager.startMonitoring();
        TEST_ASSERT(spy.count() >= 1, "应该发射 monitoringStateChanged 信号");
        
        // 检查信号参数
        QList<QVariant> args = spy.takeLast();
        TEST_ASSERT(args.at(0).toBool() == true, "信号参数应该是 true");
        
        // 停止监听
        manager.stopMonitoring();
        TEST_ASSERT(spy.count() >= 1, "应该再次发射信号");
        args = spy.takeLast();
        TEST_ASSERT(args.at(0).toBool() == false, "信号参数应该是 false");
        
        TEST_PASS("testMonitoringStateSignal: 监听状态信号正常");
        return true;
    }
    
    // ========== 配置测试 ==========
    
    bool testSetEnabled() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 禁用
        manager.setEnabled(false);
        TEST_ASSERT(!manager.isEnabled(), "应该已禁用");
        
        // 禁用时启动监听应该失败
        bool result = manager.startMonitoring();
        TEST_ASSERT(!result, "禁用时启动监听应该失败");
        TEST_ASSERT(!manager.isMonitoring(), "应该未监听");
        
        // 启用
        manager.setEnabled(true);
        TEST_ASSERT(manager.isEnabled(), "应该已启用");
        
        TEST_PASS("testSetEnabled: 启用/禁用设置正常");
        return true;
    }
    
    bool testSetMaxAgeDays() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        
        manager.setMaxAgeDays(7);
        TEST_ASSERT(manager.getMaxAgeDays() == 7, "应该是 7 天");
        
        manager.setMaxAgeDays(30);
        TEST_ASSERT(manager.getMaxAgeDays() == 30, "应该是 30 天");
        
        // 负数应该被处理为 0
        manager.setMaxAgeDays(-1);
        TEST_ASSERT(manager.getMaxAgeDays() == 0, "负数应该变为 0");
        
        TEST_PASS("testSetMaxAgeDays: 最大保留天数设置正常");
        return true;
    }
    
    bool testSetMaxCount() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        
        manager.setMaxCount(500);
        TEST_ASSERT(manager.getMaxCount() == 500, "应该是 500");
        
        manager.setMaxCount(1000);
        TEST_ASSERT(manager.getMaxCount() == 1000, "应该是 1000");
        
        // 负数应该被处理为 0
        manager.setMaxCount(-1);
        TEST_ASSERT(manager.getMaxCount() == 0, "负数应该变为 0");
        
        TEST_PASS("testSetMaxCount: 最大保留条数设置正常");
        return true;
    }
    
    // ========== 历史记录管理测试 ==========
    
    bool testGetHistoryEmpty() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        auto history = manager.getHistory();
        TEST_ASSERT(history.empty(), "空历史应该返回空列表");
        TEST_ASSERT(manager.getRecordCount() == 0, "记录数应该是 0");
        
        TEST_PASS("testGetHistoryEmpty: 空历史查询正常");
        return true;
    }
    
    bool testGetHistoryWithRecords() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加测试记录
        addTestTextRecord("Record 1", "hash_hist_001");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        addTestTextRecord("Record 2", "hash_hist_002");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        addTestTextRecord("Record 3", "hash_hist_003");
        
        // 获取历史
        auto history = manager.getHistory();
        TEST_ASSERT(history.size() == 3, "应该有 3 条记录");
        TEST_ASSERT(history[0].content == "Record 3", "第一条应该是最新的");
        TEST_ASSERT(history[2].content == "Record 1", "最后一条应该是最旧的");
        
        // 测试分页
        auto page = manager.getHistory(2, 0);
        TEST_ASSERT(page.size() == 2, "第一页应该有 2 条");
        
        page = manager.getHistory(2, 2);
        TEST_ASSERT(page.size() == 1, "第二页应该有 1 条");
        
        TEST_PASS("testGetHistoryWithRecords: 历史记录查询正常");
        return true;
    }
    
    bool testSearchRecords() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加测试记录
        addTestTextRecord("Hello World", "hash_search_001");
        addTestTextRecord("Hello China", "hash_search_002");
        addTestTextRecord("Goodbye World", "hash_search_003");
        
        // 搜索
        auto results = manager.search("Hello");
        TEST_ASSERT(results.size() == 2, "搜索 Hello 应该返回 2 条");
        
        results = manager.search("World");
        TEST_ASSERT(results.size() == 2, "搜索 World 应该返回 2 条");
        
        results = manager.search("NotExist");
        TEST_ASSERT(results.empty(), "搜索不存在的关键词应该返回空");
        
        TEST_PASS("testSearchRecords: 搜索功能正常");
        return true;
    }
    
    bool testDeleteRecord() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        int64_t id = addTestTextRecord("Delete me", "hash_del_001");
        TEST_ASSERT(manager.getRecordCount() == 1, "应该有 1 条记录");
        
        // 删除记录
        bool result = manager.deleteRecord(id);
        TEST_ASSERT(result, "删除应该成功");
        TEST_ASSERT(manager.getRecordCount() == 0, "应该没有记录了");
        
        // 删除不存在的记录
        result = manager.deleteRecord(99999);
        TEST_ASSERT(!result, "删除不存在的记录应该失败");
        
        TEST_PASS("testDeleteRecord: 删除记录正常");
        return true;
    }
    
    bool testClearHistory() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        addTestTextRecord("Clear 1", "hash_clear_001");
        addTestTextRecord("Clear 2", "hash_clear_002");
        TEST_ASSERT(manager.getRecordCount() == 2, "应该有 2 条记录");
        
        // 清空
        bool result = manager.clearHistory();
        TEST_ASSERT(result, "清空应该成功");
        TEST_ASSERT(manager.getRecordCount() == 0, "应该没有记录了");
        
        TEST_PASS("testClearHistory: 清空历史正常");
        return true;
    }
    
    // ========== 粘贴测试 ==========
    
    bool testPasteTextRecord() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        int64_t id = addTestTextRecord("Paste me", "hash_paste_001");
        
        // 获取原始最后使用时间
        auto record = suyan::ClipboardStore::instance().getRecord(id);
        int64_t originalLastUsed = record->lastUsedAt;
        
        // 等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // 监听 pasteCompleted 信号
        QSignalSpy spy(&manager, &suyan::ClipboardManager::pasteCompleted);
        
        // 粘贴
        bool result = manager.pasteRecord(id);
        TEST_ASSERT(result, "粘贴应该成功");
        
        // 验证 pasteCompleted 信号已发射
        TEST_ASSERT(spy.count() == 1, "应该发射 pasteCompleted 信号");
        QList<QVariant> args = spy.takeFirst();
        TEST_ASSERT(args.at(0).toLongLong() == id, "信号参数应该是记录 ID");
        TEST_ASSERT(args.at(1).toBool() == true, "信号参数应该是 true（成功）");
        
        // 验证最后使用时间已更新
        record = suyan::ClipboardStore::instance().getRecord(id);
        TEST_ASSERT(record->lastUsedAt >= originalLastUsed, "最后使用时间应该已更新");
        
        TEST_PASS("testPasteTextRecord: 粘贴文本记录正常");
        return true;
    }
    
    bool testPasteNonexistentRecord() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 监听 pasteCompleted 信号
        QSignalSpy spy(&manager, &suyan::ClipboardManager::pasteCompleted);
        
        // 粘贴不存在的记录
        bool result = manager.pasteRecord(99999);
        TEST_ASSERT(!result, "粘贴不存在的记录应该失败");
        
        // 验证 pasteCompleted 信号已发射（失败情况）
        TEST_ASSERT(spy.count() == 1, "应该发射 pasteCompleted 信号");
        QList<QVariant> args = spy.takeFirst();
        TEST_ASSERT(args.at(0).toLongLong() == 99999, "信号参数应该是记录 ID");
        TEST_ASSERT(args.at(1).toBool() == false, "信号参数应该是 false（失败）");
        
        TEST_PASS("testPasteNonexistentRecord: 粘贴不存在记录处理正常");
        return true;
    }
    
    // ========== 清理测试 ==========
    
    bool testPerformCleanup() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        for (int i = 1; i <= 5; i++) {
            addTestTextRecord("Cleanup " + std::to_string(i), 
                             "hash_cleanup_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        TEST_ASSERT(manager.getRecordCount() == 5, "应该有 5 条记录");
        
        // 设置保留策略（保留 3 条）
        manager.setMaxAgeDays(0);  // 不限制时间
        manager.setMaxCount(3);
        
        // 执行清理
        manager.performCleanup();
        TEST_ASSERT(manager.getRecordCount() == 3, "清理后应该剩 3 条记录");
        
        // 验证保留的是最新的
        auto history = manager.getHistory();
        TEST_ASSERT(history[0].content == "Cleanup 5", "最新的应该保留");
        
        TEST_PASS("testPerformCleanup: 清理功能正常");
        return true;
    }
    
    bool testPerformCleanupWithImages() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 先添加 2 条图片记录（这些会是最旧的，会被清理）
        std::vector<std::string> imagePaths;
        std::vector<std::string> thumbnailPaths;
        
        for (int i = 1; i <= 2; i++) {
            std::string hash = "hash_cleanup_img_" + std::to_string(i);
            
            // 创建简单的测试图片数据（1x1 PNG）
            std::vector<uint8_t> pngData = {
                0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
                0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
                0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
                0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F,
                0x00, 0x05, 0xFE, 0x02, 0xFE, 0xDC, 0xCC, 0x59,
                0xE7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
                0x44, 0xAE, 0x42, 0x60, 0x82
            };
            
            auto result = suyan::ImageStorage::instance().saveImage(pngData, "png", hash);
            TEST_ASSERT(result.success, "保存图片应该成功");
            
            imagePaths.push_back(result.imagePath);
            thumbnailPaths.push_back(result.thumbnailPath);
            
            // 添加图片记录到数据库
            suyan::ClipboardRecord record;
            record.type = suyan::ClipboardContentType::Image;
            record.content = result.imagePath;
            record.contentHash = hash;
            record.sourceApp = "com.test.cleanup";
            record.thumbnailPath = result.thumbnailPath;
            record.imageFormat = "png";
            record.imageWidth = result.width;
            record.imageHeight = result.height;
            record.fileSize = result.fileSize;
            
            int64_t id = suyan::ClipboardStore::instance().addRecord(record);
            TEST_ASSERT(id > 0, "添加图片记录应该成功");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        // 再添加 3 条文本记录（这些会是最新的，会被保留）
        for (int i = 1; i <= 3; i++) {
            addTestTextRecord("Text " + std::to_string(i), 
                             "hash_cleanup_img_text_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        TEST_ASSERT(manager.getRecordCount() == 5, "应该有 5 条记录");
        
        // 验证图片文件存在
        for (const auto& path : imagePaths) {
            TEST_ASSERT(fs::exists(path), "图片文件应该存在");
        }
        for (const auto& path : thumbnailPaths) {
            if (!path.empty()) {
                TEST_ASSERT(fs::exists(path), "缩略图文件应该存在");
            }
        }
        
        // 设置保留策略（保留 3 条，最旧的 2 条图片记录会被删除）
        manager.setMaxAgeDays(0);  // 不限制时间
        manager.setMaxCount(3);
        
        // 执行清理
        manager.performCleanup();
        TEST_ASSERT(manager.getRecordCount() == 3, "清理后应该剩 3 条记录");
        
        // 验证最旧的图片文件已被删除
        for (const auto& path : imagePaths) {
            TEST_ASSERT(!fs::exists(path), "图片文件应该已被删除");
        }
        for (const auto& path : thumbnailPaths) {
            if (!path.empty()) {
                TEST_ASSERT(!fs::exists(path), "缩略图文件应该已被删除");
            }
        }
        
        // 验证保留的记录都是文本记录
        auto history = manager.getHistory();
        TEST_ASSERT(history.size() == 3, "应该有 3 条记录");
        for (const auto& record : history) {
            TEST_ASSERT(record.type == suyan::ClipboardContentType::Text, 
                       "保留的应该都是文本记录");
        }
        
        TEST_PASS("testPerformCleanupWithImages: 清理图片记录功能正常");
        return true;
    }
    
    // ========== 信号测试 ==========
    
    bool testRecordDeletedSignal() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        int64_t id = addTestTextRecord("Signal test", "hash_signal_001");
        
        // 监听信号
        QSignalSpy spy(&manager, &suyan::ClipboardManager::recordDeleted);
        
        // 删除记录
        manager.deleteRecord(id);
        
        TEST_ASSERT(spy.count() == 1, "应该发射 recordDeleted 信号");
        QList<QVariant> args = spy.takeFirst();
        TEST_ASSERT(args.at(0).toLongLong() == id, "信号参数应该是记录 ID");
        
        TEST_PASS("testRecordDeletedSignal: 删除信号正常");
        return true;
    }
    
    bool testHistoryClearedSignal() {
        if (!ensureInitialized()) {
            TEST_ASSERT(false, "初始化失败");
        }
        resetTestEnvironment();
        
        auto& manager = suyan::ClipboardManager::instance();
        
        // 添加记录
        addTestTextRecord("Clear signal", "hash_clearsig_001");
        
        // 监听信号
        QSignalSpy spy(&manager, &suyan::ClipboardManager::historyCleared);
        
        // 清空历史
        manager.clearHistory();
        
        TEST_ASSERT(spy.count() == 1, "应该发射 historyCleared 信号");
        
        TEST_PASS("testHistoryClearedSignal: 清空信号正常");
        return true;
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    ClipboardManagerTest test;
    return test.runAllTests() ? 0 : 1;
}
