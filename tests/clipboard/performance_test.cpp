/**
 * 剪贴板模块性能测试
 *
 * 验证 Task 20 的性能优化目标：
 * - 搜索性能：大数据量下搜索响应 < 100ms
 * - 窗口显示性能：首次显示延迟 < 100ms（需要 GUI 环境）
 * - 列表滚动性能：虚拟化渲染优化
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>
#include <random>
#include <string>
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

class PerformanceTest {
public:
    PerformanceTest() {
        testDataDir_ = fs::temp_directory_path().string() + "/suyan_perf_test";
        testDbPath_ = testDataDir_ + "/clipboard.db";
        fs::remove_all(testDataDir_);
    }
    
    ~PerformanceTest() {
        suyan::ClipboardStore::instance().shutdown();
        fs::remove_all(testDataDir_);
    }
    
    bool runAllTests() {
        std::cout << "=== 剪贴板模块性能测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testDataDir_ << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        allPassed &= testSearchPerformance();
        allPassed &= testBulkInsertPerformance();
        allPassed &= testPaginationPerformance();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有性能测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分性能测试失败 ===" << std::endl;
        }
        
        return allPassed;
    }
    
private:
    std::string testDataDir_;
    std::string testDbPath_;
    
    // 生成随机文本
    std::string generateRandomText(int length) {
        static const char charset[] = 
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789 ";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
        
        std::string result;
        result.reserve(length);
        for (int i = 0; i < length; ++i) {
            result += charset[dis(gen)];
        }
        return result;
    }
    
    // 生成唯一哈希
    std::string generateHash(int index) {
        return "perf_hash_" + std::to_string(index);
    }
    
    // 创建测试记录
    suyan::ClipboardRecord createTextRecord(const std::string& content, 
                                             const std::string& hash) {
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = content;
        record.contentHash = hash;
        record.sourceApp = "com.test.perf";
        return record;
    }
    
    /**
     * 测试搜索性能
     * 目标：1000 条记录下搜索响应 < 100ms
     */
    bool testSearchPerformance() {
        std::cout << "--- 搜索性能测试 ---" << std::endl;
        
        auto& store = suyan::ClipboardStore::instance();
        store.initialize(testDbPath_);
        store.clearAll();
        
        const int RECORD_COUNT = 1000;
        const int SEARCH_ITERATIONS = 10;
        
        // 插入测试数据
        std::cout << "  插入 " << RECORD_COUNT << " 条测试记录..." << std::endl;
        auto insertStart = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < RECORD_COUNT; ++i) {
            // 每 10 条记录包含 "searchable" 关键词
            std::string content;
            if (i % 10 == 0) {
                content = "This is a searchable record " + std::to_string(i) + " " + generateRandomText(50);
            } else {
                content = "Normal record " + std::to_string(i) + " " + generateRandomText(50);
            }
            store.addRecord(createTextRecord(content, generateHash(i)));
        }
        
        auto insertEnd = std::chrono::high_resolution_clock::now();
        auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(insertEnd - insertStart);
        std::cout << "  插入耗时: " << insertDuration.count() << "ms" << std::endl;
        
        // 测试搜索性能
        std::cout << "  执行 " << SEARCH_ITERATIONS << " 次搜索..." << std::endl;
        
        long long totalSearchTime = 0;
        long long maxSearchTime = 0;
        
        for (int i = 0; i < SEARCH_ITERATIONS; ++i) {
            auto searchStart = std::chrono::high_resolution_clock::now();
            auto results = store.searchText("searchable", 100);
            auto searchEnd = std::chrono::high_resolution_clock::now();
            
            auto searchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEnd - searchStart);
            totalSearchTime += searchDuration.count();
            maxSearchTime = std::max(maxSearchTime, searchDuration.count());
            
            // 验证搜索结果
            TEST_ASSERT(results.size() > 0, "搜索应该返回结果");
        }
        
        long long avgSearchTime = totalSearchTime / SEARCH_ITERATIONS;
        std::cout << "  平均搜索耗时: " << avgSearchTime << "ms" << std::endl;
        std::cout << "  最大搜索耗时: " << maxSearchTime << "ms" << std::endl;
        
        // 性能目标：搜索响应 < 100ms
        TEST_ASSERT(maxSearchTime < 100, "搜索响应应该 < 100ms");
        
        TEST_PASS("testSearchPerformance: 搜索性能达标 (< 100ms)");
        return true;
    }
    
    /**
     * 测试批量插入性能
     */
    bool testBulkInsertPerformance() {
        std::cout << "--- 批量插入性能测试 ---" << std::endl;
        
        auto& store = suyan::ClipboardStore::instance();
        store.clearAll();
        
        const int RECORD_COUNT = 500;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < RECORD_COUNT; ++i) {
            std::string content = "Bulk insert record " + std::to_string(i) + " " + generateRandomText(100);
            store.addRecord(createTextRecord(content, "bulk_" + generateHash(i)));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  插入 " << RECORD_COUNT << " 条记录耗时: " << duration.count() << "ms" << std::endl;
        std::cout << "  平均每条: " << (double)duration.count() / RECORD_COUNT << "ms" << std::endl;
        
        // 验证记录数
        TEST_ASSERT(store.getRecordCount() == RECORD_COUNT, "记录数应该正确");
        
        // 性能目标：平均每条插入 < 10ms
        TEST_ASSERT(duration.count() < RECORD_COUNT * 10, "批量插入性能应该达标");
        
        TEST_PASS("testBulkInsertPerformance: 批量插入性能达标");
        return true;
    }
    
    /**
     * 测试分页查询性能
     */
    bool testPaginationPerformance() {
        std::cout << "--- 分页查询性能测试 ---" << std::endl;
        
        auto& store = suyan::ClipboardStore::instance();
        // 使用上一个测试的数据
        
        const int PAGE_SIZE = 50;
        const int ITERATIONS = 20;
        
        long long totalTime = 0;
        long long maxTime = 0;
        
        for (int i = 0; i < ITERATIONS; ++i) {
            int offset = (i % 10) * PAGE_SIZE;  // 测试不同页
            
            auto start = std::chrono::high_resolution_clock::now();
            auto records = store.getAllRecords(PAGE_SIZE, offset);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            totalTime += duration.count();
            maxTime = std::max(maxTime, duration.count());
        }
        
        long long avgTime = totalTime / ITERATIONS;
        std::cout << "  平均分页查询耗时: " << avgTime << "ms" << std::endl;
        std::cout << "  最大分页查询耗时: " << maxTime << "ms" << std::endl;
        
        // 性能目标：分页查询 < 50ms
        TEST_ASSERT(maxTime < 50, "分页查询应该 < 50ms");
        
        TEST_PASS("testPaginationPerformance: 分页查询性能达标 (< 50ms)");
        return true;
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    PerformanceTest test;
    return test.runAllTests() ? 0 : 1;
}
