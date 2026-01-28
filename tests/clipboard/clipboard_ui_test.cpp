/**
 * ClipboardWindow UI 层测试
 *
 * 验证 UI 层功能：
 * - ClipboardWindow 显示隐藏、屏幕居中定位
 * - 列表正确渲染文本和图片记录
 * - 搜索过滤实时生效
 * - 点击记录触发 recordSelected 信号
 *
 * Requirements: UI 层验证 (Task 13 Checkpoint)
 */

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <QApplication>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QImage>
#include <QBuffer>
#include <QListWidget>
#include <QLineEdit>

#include "clipboard_window.h"
#include "clipboard_list.h"
#include "clipboard_item_widget.h"
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

class ClipboardUITest : public QObject {
    Q_OBJECT

public:
    ClipboardUITest() {
        testDataDir_ = fs::temp_directory_path().string() + "/suyan_clipboard_ui_test";
        fs::remove_all(testDataDir_);
    }

    ~ClipboardUITest() {
        suyan::ClipboardManager::instance().shutdown();
        fs::remove_all(testDataDir_);
    }

    bool runAllTests() {
        std::cout << "=== ClipboardWindow UI 层测试 ===" << std::endl;
        std::cout << "测试数据目录: " << testDataDir_ << std::endl;
        std::cout << std::endl;

        bool allPassed = true;

        // 初始化测试环境
        allPassed &= testInitialize();
        if (!allPassed) {
            std::cout << "初始化失败，无法继续测试" << std::endl;
            return false;
        }

        // 窗口显示隐藏测试
        allPassed &= testWindowShowHide();
        allPassed &= testWindowToggleVisibility();
        allPassed &= testWindowCenterOnScreen();
        allPassed &= testWindowEscapeHide();

        // 列表渲染测试
        allPassed &= testListRenderTextRecords();
        allPassed &= testListRenderImageRecords();
        allPassed &= testListEmptyHint();

        // 搜索过滤测试
        allPassed &= testSearchFilter();
        allPassed &= testSearchNoResults();
        allPassed &= testSearchClear();

        // 信号测试
        allPassed &= testRecordSelectedSignal();
        allPassed &= testWindowHiddenSignal();
        allPassed &= testWindowShownSignal();

        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有 UI 测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }

        return allPassed;
    }

private:
    std::string testDataDir_;
    std::unique_ptr<suyan::ClipboardWindow> window_;

    // 初始化测试环境
    bool testInitialize() {
        std::cout << "--- 初始化测试环境 ---" << std::endl;

        auto& manager = suyan::ClipboardManager::instance();
        bool result = manager.initialize(testDataDir_);

        if (!result) {
            std::cout << "✗ ClipboardManager 初始化失败" << std::endl;
            return false;
        }

        // 清空历史
        manager.clearHistory();

        // 创建窗口
        window_ = std::make_unique<suyan::ClipboardWindow>();

        TEST_PASS("测试环境初始化成功");
        return true;
    }

    // 重置测试数据
    void resetTestData() {
        suyan::ClipboardManager::instance().clearHistory();
    }

    // 添加测试文本记录
    int64_t addTestTextRecord(const std::string& content, const std::string& hash) {
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Text;
        record.content = content;
        record.contentHash = hash;
        record.sourceApp = "com.test.ui";
        return suyan::ClipboardStore::instance().addRecord(record);
    }

    // 添加测试图片记录
    int64_t addTestImageRecord(const std::string& hash) {
        // 创建测试图片
        QImage testImage(100, 80, QImage::Format_RGB32);
        testImage.fill(Qt::green);

        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);
        testImage.save(&buffer, "PNG");
        buffer.close();

        std::vector<uint8_t> imageBytes(imageData.begin(), imageData.end());

        // 保存图片
        auto result = suyan::ImageStorage::instance().saveImage(imageBytes, "png", hash);
        if (!result.success) {
            return -1;
        }

        // 添加数据库记录
        suyan::ClipboardRecord record;
        record.type = suyan::ClipboardContentType::Image;
        record.content = result.imagePath;
        record.contentHash = hash;
        record.sourceApp = "com.test.ui.image";
        record.thumbnailPath = result.thumbnailPath;
        record.imageFormat = "png";
        record.imageWidth = result.width;
        record.imageHeight = result.height;
        record.fileSize = result.fileSize;

        return suyan::ClipboardStore::instance().addRecord(record);
    }

    // 处理事件循环
    void processEvents(int ms = 100) {
        QTest::qWait(ms);
    }

    // ========== 窗口显示隐藏测试 ==========

    bool testWindowShowHide() {
        std::cout << std::endl << "--- 测试窗口显示隐藏 ---" << std::endl;

        // 初始状态应该隐藏
        TEST_ASSERT(!window_->isVisible(), "初始状态应该隐藏");

        // 显示窗口
        window_->showWindow();
        processEvents();
        TEST_ASSERT(window_->isVisible(), "调用 showWindow 后应该可见");

        // 隐藏窗口
        window_->hideWindow();
        processEvents();
        TEST_ASSERT(!window_->isVisible(), "调用 hideWindow 后应该隐藏");

        TEST_PASS("窗口显示隐藏正常");
        return true;
    }

    bool testWindowToggleVisibility() {
        std::cout << std::endl << "--- 测试窗口切换显示 ---" << std::endl;

        // 确保初始隐藏
        window_->hideWindow();
        processEvents();
        TEST_ASSERT(!window_->isVisible(), "初始应该隐藏");

        // 切换 -> 显示
        window_->toggleVisibility();
        processEvents();
        TEST_ASSERT(window_->isVisible(), "第一次切换应该显示");

        // 切换 -> 隐藏
        window_->toggleVisibility();
        processEvents();
        TEST_ASSERT(!window_->isVisible(), "第二次切换应该隐藏");

        TEST_PASS("窗口切换显示正常");
        return true;
    }

    bool testWindowCenterOnScreen() {
        std::cout << std::endl << "--- 测试窗口屏幕居中 ---" << std::endl;

        // 显示窗口
        window_->showWindow();
        processEvents();

        // 获取屏幕信息
        QScreen* screen = QApplication::primaryScreen();
        TEST_ASSERT(screen != nullptr, "应该能获取主屏幕");

        QRect screenGeometry = screen->availableGeometry();
        QRect windowGeometry = window_->geometry();

        // 计算期望的居中位置
        int expectedX = screenGeometry.x() + (screenGeometry.width() - windowGeometry.width()) / 2;
        int expectedY = screenGeometry.y() + (screenGeometry.height() - windowGeometry.height()) / 2;

        // 允许一定误差（不同平台可能有细微差异）
        int tolerance = 10;
        bool xCentered = std::abs(windowGeometry.x() - expectedX) <= tolerance;
        bool yCentered = std::abs(windowGeometry.y() - expectedY) <= tolerance;

        std::cout << "  屏幕可用区域: " << screenGeometry.x() << "," << screenGeometry.y()
                  << " " << screenGeometry.width() << "x" << screenGeometry.height() << std::endl;
        std::cout << "  窗口位置: " << windowGeometry.x() << "," << windowGeometry.y() << std::endl;
        std::cout << "  期望位置: " << expectedX << "," << expectedY << std::endl;

        TEST_ASSERT(xCentered, "窗口 X 坐标应该居中");
        TEST_ASSERT(yCentered, "窗口 Y 坐标应该居中");

        window_->hideWindow();
        processEvents();

        TEST_PASS("窗口屏幕居中正常");
        return true;
    }

    bool testWindowEscapeHide() {
        std::cout << std::endl << "--- 测试 Escape 键隐藏窗口 ---" << std::endl;

        // 显示窗口
        window_->showWindow();
        processEvents();
        TEST_ASSERT(window_->isVisible(), "窗口应该可见");

        // 模拟按下 Escape 键
        QTest::keyClick(window_.get(), Qt::Key_Escape);
        processEvents();

        TEST_ASSERT(!window_->isVisible(), "按 Escape 后窗口应该隐藏");

        TEST_PASS("Escape 键隐藏窗口正常");
        return true;
    }

    // ========== 列表渲染测试 ==========

    bool testListRenderTextRecords() {
        std::cout << std::endl << "--- 测试列表渲染文本记录 ---" << std::endl;

        resetTestData();

        // 添加测试文本记录
        addTestTextRecord("测试文本内容 1", "ui_text_hash_001");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        addTestTextRecord("测试文本内容 2", "ui_text_hash_002");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        addTestTextRecord("测试文本内容 3", "ui_text_hash_003");

        // 显示窗口（会刷新列表）
        window_->showWindow();
        processEvents(200);

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 验证记录数量
        int count = clipboardList->recordCount();
        std::cout << "  列表记录数: " << count << std::endl;
        TEST_ASSERT(count == 3, "列表应该有 3 条记录");

        window_->hideWindow();
        processEvents();

        TEST_PASS("列表渲染文本记录正常");
        return true;
    }

    bool testListRenderImageRecords() {
        std::cout << std::endl << "--- 测试列表渲染图片记录 ---" << std::endl;

        resetTestData();

        // 添加测试图片记录
        int64_t imageId = addTestImageRecord("ui_image_hash_001");
        TEST_ASSERT(imageId > 0, "添加图片记录应该成功");

        // 添加一条文本记录
        addTestTextRecord("混合测试文本", "ui_mixed_hash_001");

        // 显示窗口
        window_->showWindow();
        processEvents(200);

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 验证记录数量
        int count = clipboardList->recordCount();
        std::cout << "  列表记录数: " << count << std::endl;
        TEST_ASSERT(count == 2, "列表应该有 2 条记录（1 图片 + 1 文本）");

        window_->hideWindow();
        processEvents();

        TEST_PASS("列表渲染图片记录正常");
        return true;
    }

    bool testListEmptyHint() {
        std::cout << std::endl << "--- 测试空列表提示 ---" << std::endl;

        resetTestData();

        // 显示窗口（空列表）
        window_->showWindow();
        processEvents(200);

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 验证记录数量为 0
        int count = clipboardList->recordCount();
        std::cout << "  列表记录数: " << count << std::endl;
        TEST_ASSERT(count == 0, "空列表应该有 0 条记录");

        window_->hideWindow();
        processEvents();

        TEST_PASS("空列表提示正常");
        return true;
    }

    // ========== 搜索过滤测试 ==========

    bool testSearchFilter() {
        std::cout << std::endl << "--- 测试搜索过滤 ---" << std::endl;

        resetTestData();

        // 添加测试记录
        addTestTextRecord("Hello World", "ui_search_hash_001");
        addTestTextRecord("Hello China", "ui_search_hash_002");
        addTestTextRecord("Goodbye World", "ui_search_hash_003");

        // 显示窗口
        window_->showWindow();
        processEvents(200);

        // 获取搜索框
        auto* searchEdit = window_->findChild<QLineEdit*>();
        TEST_ASSERT(searchEdit != nullptr, "应该能找到搜索框");

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 初始应该有 3 条记录
        TEST_ASSERT(clipboardList->recordCount() == 3, "初始应该有 3 条记录");

        // 输入搜索关键词
        searchEdit->setText("Hello");
        processEvents(500);  // 等待防抖

        // 验证过滤结果
        int filteredCount = clipboardList->recordCount();
        std::cout << "  搜索 'Hello' 后记录数: " << filteredCount << std::endl;
        TEST_ASSERT(filteredCount == 2, "搜索 'Hello' 应该返回 2 条记录");

        window_->hideWindow();
        processEvents();

        TEST_PASS("搜索过滤正常");
        return true;
    }

    bool testSearchNoResults() {
        std::cout << std::endl << "--- 测试搜索无结果 ---" << std::endl;

        resetTestData();

        // 添加测试记录
        addTestTextRecord("测试内容", "ui_noresult_hash_001");

        // 显示窗口
        window_->showWindow();
        processEvents(200);

        // 获取搜索框
        auto* searchEdit = window_->findChild<QLineEdit*>();
        TEST_ASSERT(searchEdit != nullptr, "应该能找到搜索框");

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 搜索不存在的关键词
        searchEdit->setText("NotExistKeyword");
        processEvents(500);

        // 验证无结果
        int count = clipboardList->recordCount();
        std::cout << "  搜索不存在关键词后记录数: " << count << std::endl;
        TEST_ASSERT(count == 0, "搜索不存在的关键词应该返回 0 条记录");

        window_->hideWindow();
        processEvents();

        TEST_PASS("搜索无结果处理正常");
        return true;
    }

    bool testSearchClear() {
        std::cout << std::endl << "--- 测试清空搜索 ---" << std::endl;

        resetTestData();

        // 添加测试记录
        addTestTextRecord("搜索测试 1", "ui_clear_hash_001");
        addTestTextRecord("搜索测试 2", "ui_clear_hash_002");

        // 显示窗口
        window_->showWindow();
        processEvents(200);

        // 获取搜索框和列表
        auto* searchEdit = window_->findChild<QLineEdit*>();
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(searchEdit != nullptr && clipboardList != nullptr, "应该能找到组件");

        // 先搜索
        searchEdit->setText("测试 1");
        processEvents(500);
        int filteredCount = clipboardList->recordCount();
        std::cout << "  搜索后记录数: " << filteredCount << std::endl;

        // 清空搜索
        searchEdit->clear();
        processEvents(500);

        // 验证恢复全部记录
        int restoredCount = clipboardList->recordCount();
        std::cout << "  清空搜索后记录数: " << restoredCount << std::endl;
        TEST_ASSERT(restoredCount == 2, "清空搜索后应该恢复全部记录");

        window_->hideWindow();
        processEvents();

        TEST_PASS("清空搜索正常");
        return true;
    }

    // ========== 信号测试 ==========

    bool testRecordSelectedSignal() {
        std::cout << std::endl << "--- 测试 recordSelected 信号 ---" << std::endl;

        resetTestData();

        // 添加测试记录
        int64_t recordId = addTestTextRecord("信号测试内容", "ui_signal_hash_001");
        TEST_ASSERT(recordId > 0, "添加记录应该成功");

        // 显示窗口
        window_->showWindow();
        processEvents(200);

        // 监听信号
        QSignalSpy spy(window_.get(), &suyan::ClipboardWindow::recordSelected);

        // 获取列表组件
        auto* clipboardList = window_->findChild<suyan::ClipboardList*>();
        TEST_ASSERT(clipboardList != nullptr, "应该能找到 ClipboardList 组件");

        // 模拟点击记录（通过发射 itemSelected 信号）
        emit clipboardList->itemSelected(recordId);
        processEvents(100);

        // 验证信号
        TEST_ASSERT(spy.count() >= 1, "应该发射 recordSelected 信号");
        QList<QVariant> args = spy.takeLast();
        TEST_ASSERT(args.at(0).toLongLong() == recordId, "信号参数应该是记录 ID");

        // 注意：窗口隐藏是由 ClipboardManager::pasteCompleted 信号触发的
        // 在实际应用中，main.mm 中连接了 pasteCompleted -> hideWindow
        // 在单元测试环境中，我们只验证 recordSelected 信号正确发射
        // 窗口隐藏行为在集成测试中验证
        
        // 手动隐藏窗口以清理测试状态
        window_->hideWindow();
        processEvents();

        TEST_PASS("recordSelected 信号正常");
        return true;
    }

    bool testWindowHiddenSignal() {
        std::cout << std::endl << "--- 测试 windowHidden 信号 ---" << std::endl;

        // 显示窗口
        window_->showWindow();
        processEvents();

        // 监听信号
        QSignalSpy spy(window_.get(), &suyan::ClipboardWindow::windowHidden);

        // 隐藏窗口
        window_->hideWindow();
        processEvents();

        TEST_ASSERT(spy.count() == 1, "应该发射 windowHidden 信号");

        TEST_PASS("windowHidden 信号正常");
        return true;
    }

    bool testWindowShownSignal() {
        std::cout << std::endl << "--- 测试 windowShown 信号 ---" << std::endl;

        // 确保窗口隐藏
        window_->hideWindow();
        processEvents();

        // 监听信号
        QSignalSpy spy(window_.get(), &suyan::ClipboardWindow::windowShown);

        // 显示窗口
        window_->showWindow();
        processEvents();

        TEST_ASSERT(spy.count() == 1, "应该发射 windowShown 信号");

        window_->hideWindow();
        processEvents();

        TEST_PASS("windowShown 信号正常");
        return true;
    }
};

#include "clipboard_ui_test.moc"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    ClipboardUITest test;
    return test.runAllTests() ? 0 : 1;
}
