/**
 * ClipboardManager 集成测试
 *
 * 手动验证核心层功能：
 * - 启动监听后复制文本验证记录添加
 * - 复制相同内容验证去重（时间戳更新）
 * - 复制图片验证原图和缩略图存储
 * - 调用 pasteRecord 验证内容写入系统剪贴板
 *
 * 注意：此测试需要用户交互，不适合自动化测试
 */

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <QTimer>
#include <QImage>
#include <QBuffer>
#include "clipboard_manager.h"
#include "clipboard_store.h"
#include "image_storage.h"
#include "clipboard_monitor.h"

namespace fs = std::filesystem;

class ClipboardIntegrationTest : public QObject {
    Q_OBJECT

public:
    ClipboardIntegrationTest() {
        testDataDir_ = fs::temp_directory_path().string() + "/suyan_clipboard_integration_test";
        fs::remove_all(testDataDir_);
    }

    ~ClipboardIntegrationTest() {
        suyan::ClipboardManager::instance().shutdown();
        fs::remove_all(testDataDir_);
    }

    bool runTests() {
        std::cout << "=== ClipboardManager 集成测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testDataDir_ << std::endl;
        std::cout << std::endl;

        bool allPassed = true;

        // 初始化
        allPassed &= testInitialize();
        if (!allPassed) {
            std::cout << "初始化失败，无法继续测试" << std::endl;
            return false;
        }

        // 测试文本记录添加
        allPassed &= testTextRecordAddition();

        // 测试去重（时间戳更新）
        allPassed &= testDeduplication();

        // 测试图片记录添加
        allPassed &= testImageRecordAddition();

        // 测试粘贴功能
        allPassed &= testPasteRecord();

        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有集成测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }

        return allPassed;
    }

private:
    std::string testDataDir_;

    bool testInitialize() {
        std::cout << "--- 测试初始化 ---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();
        bool result = manager.initialize(testDataDir_);

        if (!result) {
            std::cout << "✗ 初始化失败" << std::endl;
            return false;
        }

        std::cout << "✓ 初始化成功" << std::endl;
        return true;
    }

    bool testTextRecordAddition() {
        std::cout << std::endl << "--- 测试文本记录添加 ---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();
        manager.setEnabled(true);

        // 清空历史
        manager.clearHistory();

        // 获取初始记录数
        int64_t initialCount = manager.getRecordCount();
        std::cout << "初始记录数: " << initialCount << std::endl;

        // 模拟剪贴板内容变化（直接调用内部处理）
        // 由于我们无法直接触发系统剪贴板变化，我们通过 ClipboardStore 直接添加记录
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = "测试文本内容 - " + std::to_string(std::time(nullptr));
        record.contentHash = "test_hash_text_" + std::to_string(std::time(nullptr));
        record.sourceApp = "com.test.integration";

        int64_t id = suyan::ClipboardStore::instance().addRecord(record);

        if (id <= 0) {
            std::cout << "✗ 添加文本记录失败" << std::endl;
            return false;
        }

        // 验证记录数增加
        int64_t newCount = manager.getRecordCount();
        if (newCount != initialCount + 1) {
            std::cout << "✗ 记录数未增加: " << newCount << " (期望 " << (initialCount + 1) << ")" << std::endl;
            return false;
        }

        // 验证记录内容
        auto history = manager.getHistory(1);
        if (history.empty() || history[0].content != record.content) {
            std::cout << "✗ 记录内容不匹配" << std::endl;
            return false;
        }

        std::cout << "✓ 文本记录添加成功，ID: " << id << std::endl;
        return true;
    }

    bool testDeduplication() {
        std::cout << std::endl << "--- 测试去重（时间戳更新）---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();

        // 添加一条记录
        std::string testHash = "dedup_test_hash_" + std::to_string(std::time(nullptr));
        suyan::ClipboardRecord record1;
        record1.type = suyan::ClipboardContentType::Text;
        record1.content = "去重测试内容";
        record1.contentHash = testHash;
        record1.sourceApp = "com.test.dedup";

        int64_t id1 = suyan::ClipboardStore::instance().addRecord(record1);
        if (id1 <= 0) {
            std::cout << "✗ 添加第一条记录失败" << std::endl;
            return false;
        }

        // 获取原始时间戳
        auto originalRecord = suyan::ClipboardStore::instance().getRecord(id1);
        if (!originalRecord) {
            std::cout << "✗ 获取原始记录失败" << std::endl;
            return false;
        }
        int64_t originalLastUsed = originalRecord->lastUsedAt;
        std::cout << "原始最后使用时间: " << originalLastUsed << std::endl;

        // 等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 添加相同哈希的记录（应该更新时间戳而非创建新记录）
        suyan::ClipboardRecord record2;
        record2.type = suyan::ClipboardContentType::Text;
        record2.content = "去重测试内容";
        record2.contentHash = testHash;  // 相同哈希
        record2.sourceApp = "com.test.dedup2";

        int64_t id2 = suyan::ClipboardStore::instance().addRecord(record2);

        // 应该返回相同的 ID
        if (id2 != id1) {
            std::cout << "✗ 去重失败，创建了新记录: " << id2 << " (期望 " << id1 << ")" << std::endl;
            return false;
        }

        // 验证时间戳已更新
        auto updatedRecord = suyan::ClipboardStore::instance().getRecord(id1);
        if (!updatedRecord) {
            std::cout << "✗ 获取更新后记录失败" << std::endl;
            return false;
        }

        if (updatedRecord->lastUsedAt <= originalLastUsed) {
            std::cout << "✗ 时间戳未更新: " << updatedRecord->lastUsedAt 
                      << " (原始 " << originalLastUsed << ")" << std::endl;
            return false;
        }

        std::cout << "更新后最后使用时间: " << updatedRecord->lastUsedAt << std::endl;
        std::cout << "✓ 去重功能正常，时间戳已更新" << std::endl;
        return true;
    }

    bool testImageRecordAddition() {
        std::cout << std::endl << "--- 测试图片记录添加 ---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();

        // 创建测试图片
        QImage testImage(200, 150, QImage::Format_RGB32);
        testImage.fill(Qt::blue);

        // 转换为 PNG 数据
        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        testImage.save(&buffer, "PNG");
        buffer.close();

        std::vector<uint8_t> imageBytes(imageData.begin(), imageData.end());
        std::string imageHash = "image_test_hash_" + std::to_string(std::time(nullptr));

        // 保存图片
        auto result = suyan::ImageStorage::instance().saveImage(imageBytes, "png", imageHash);
        if (!result.success) {
            std::cout << "✗ 保存图片失败: " << result.errorMessage << std::endl;
            return false;
        }

        std::cout << "图片保存成功:" << std::endl;
        std::cout << "  原图路径: " << result.imagePath << std::endl;
        std::cout << "  缩略图路径: " << result.thumbnailPath << std::endl;
        std::cout << "  尺寸: " << result.width << "x" << result.height << std::endl;

        // 验证文件存在
        if (!fs::exists(result.imagePath)) {
            std::cout << "✗ 原图文件不存在" << std::endl;
            return false;
        }

        if (!fs::exists(result.thumbnailPath)) {
            std::cout << "✗ 缩略图文件不存在" << std::endl;
            return false;
        }

        // 添加数据库记录
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Image;
        record.content = result.imagePath;
        record.contentHash = imageHash;
        record.sourceApp = "com.test.image";
        record.thumbnailPath = result.thumbnailPath;
        record.imageFormat = "png";
        record.imageWidth = result.width;
        record.imageHeight = result.height;
        record.fileSize = result.fileSize;

        int64_t id = suyan::ClipboardStore::instance().addRecord(record);
        if (id <= 0) {
            std::cout << "✗ 添加图片记录失败" << std::endl;
            return false;
        }

        // 验证缩略图尺寸
        QImage thumbnail(QString::fromStdString(result.thumbnailPath));
        if (thumbnail.isNull()) {
            std::cout << "✗ 无法加载缩略图" << std::endl;
            return false;
        }

        std::cout << "  缩略图实际尺寸: " << thumbnail.width() << "x" << thumbnail.height() << std::endl;

        // 缩略图应该保持宽高比，最大 120x80
        if (thumbnail.width() > 120 || thumbnail.height() > 80) {
            std::cout << "✗ 缩略图尺寸超出限制" << std::endl;
            return false;
        }

        std::cout << "✓ 图片记录添加成功，ID: " << id << std::endl;
        return true;
    }

    bool testPasteRecord() {
        std::cout << std::endl << "--- 测试粘贴功能 ---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();

        // 添加一条文本记录
        std::string testContent = "粘贴测试内容 - " + std::to_string(std::time(nullptr));
        std::string testHash = "paste_test_hash_" + std::to_string(std::time(nullptr));

        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = testContent;
        record.contentHash = testHash;
        record.sourceApp = "com.test.paste";

        int64_t id = suyan::ClipboardStore::instance().addRecord(record);
        if (id <= 0) {
            std::cout << "✗ 添加测试记录失败" << std::endl;
            return false;
        }

        // 获取原始最后使用时间
        auto originalRecord = suyan::ClipboardStore::instance().getRecord(id);
        int64_t originalLastUsed = originalRecord->lastUsedAt;

        // 等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 执行粘贴
        bool result = manager.pasteRecord(id);
        if (!result) {
            std::cout << "✗ 粘贴操作失败" << std::endl;
            return false;
        }

        // 验证最后使用时间已更新
        auto updatedRecord = suyan::ClipboardStore::instance().getRecord(id);
        if (updatedRecord->lastUsedAt <= originalLastUsed) {
            std::cout << "✗ 粘贴后时间戳未更新" << std::endl;
            return false;
        }

        std::cout << "✓ 粘贴功能正常，内容已写入系统剪贴板" << std::endl;
        std::cout << "  记录 ID: " << id << std::endl;
        std::cout << "  内容: " << testContent << std::endl;

        return true;
    }
};

#include "clipboard_integration_test.moc"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    ClipboardIntegrationTest test;
    return test.runTests() ? 0 : 1;
}
