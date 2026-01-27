/**
 * 候选词窗口可见性单元测试
 * Task 9.4: 编写候选词窗口单元测试
 * 
 * 测试内容：
 * - 空候选词时窗口隐藏
 * - Property 5: 空候选词窗口自动隐藏
 * 
 * Validates: Requirements 6.4
 */

#include <QApplication>
#include <QTimer>
#include <QDir>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

#include "../../src/ui/candidate_window.h"
#include "../../src/ui/candidate_view.h"
#include "../../src/ui/theme_manager.h"
#include "../../src/ui/layout_manager.h"
#include "../../src/ui/suyan_ui_init.h"
#include "../../src/core/config_manager.h"
#include "../../src/core/input_engine.h"

using namespace suyan;

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

class CandidateWindowVisibilityTest {
public:
    CandidateWindowVisibilityTest(CandidateWindow* window) 
        : window_(window) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }

    bool runAllTests() {
        std::cout << "=== 候选词窗口可见性单元测试 ===" << std::endl;
        std::cout << "Task 9.4: 编写候选词窗口单元测试" << std::endl;
        std::cout << "Validates: Requirements 6.4" << std::endl;
        std::cout << std::endl;

        bool allPassed = true;

        allPassed &= testEmptyCandidatesHidesWindow();
        allPassed &= testClearCandidatesHidesWindow();
        allPassed &= testNonComposingHidesWindow();
        allPassed &= testProperty5_EmptyCandidatesAutoHide();

        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }

        return allPassed;
    }

private:
    /**
     * 测试空候选词列表时窗口隐藏
     */
    bool testEmptyCandidatesHidesWindow() {
        std::cout << "\n--- 空候选词隐藏测试 ---" << std::endl;
        
        // 先显示窗口并设置候选词
        InputState state;
        state.preedit = "test";
        state.rawInput = "test";
        state.highlightedIndex = 0;
        state.pageIndex = 0;
        state.pageSize = 9;
        state.hasMorePages = false;
        state.mode = InputMode::Chinese;
        state.isComposing = true;
        state.candidates = {
            {"测试", "cè shì", 1},
            {"测试2", "cè shì", 2}
        };
        
        window_->updateCandidates(state);
        window_->showAt(QPoint(100, 100));
        QCoreApplication::processEvents();
        
        TEST_ASSERT(window_->isWindowVisible(), "有候选词时窗口应该可见");
        
        // 设置空候选词
        state.candidates.clear();
        state.isComposing = false;
        window_->updateCandidates(state);
        QCoreApplication::processEvents();
        
        TEST_ASSERT(!window_->isWindowVisible(), "空候选词时窗口应该隐藏");
        
        TEST_PASS("testEmptyCandidatesHidesWindow: 空候选词正确隐藏窗口");
        return true;
    }

    /**
     * 测试 clearCandidates 方法隐藏窗口
     */
    bool testClearCandidatesHidesWindow() {
        std::cout << "\n--- clearCandidates 隐藏测试 ---" << std::endl;
        
        // 先显示窗口并设置候选词
        InputState state;
        state.preedit = "hello";
        state.rawInput = "hello";
        state.highlightedIndex = 0;
        state.pageIndex = 0;
        state.pageSize = 9;
        state.hasMorePages = false;
        state.mode = InputMode::Chinese;
        state.isComposing = true;
        state.candidates = {
            {"你好", "nǐ hǎo", 1}
        };
        
        window_->updateCandidates(state);
        window_->showAt(QPoint(200, 200));
        QCoreApplication::processEvents();
        
        TEST_ASSERT(window_->isWindowVisible(), "有候选词时窗口应该可见");
        
        // 调用 clearCandidates
        window_->clearCandidates();
        QCoreApplication::processEvents();
        
        TEST_ASSERT(!window_->isWindowVisible(), "clearCandidates 后窗口应该隐藏");
        
        TEST_PASS("testClearCandidatesHidesWindow: clearCandidates 正确隐藏窗口");
        return true;
    }

    /**
     * 测试非输入状态时窗口隐藏
     */
    bool testNonComposingHidesWindow() {
        std::cout << "\n--- 非输入状态隐藏测试 ---" << std::endl;
        
        // 先显示窗口
        InputState state;
        state.preedit = "wo";
        state.rawInput = "wo";
        state.highlightedIndex = 0;
        state.pageIndex = 0;
        state.pageSize = 9;
        state.hasMorePages = false;
        state.mode = InputMode::Chinese;
        state.isComposing = true;
        state.candidates = {
            {"我", "wǒ", 1},
            {"窝", "wō", 2}
        };
        
        window_->updateCandidates(state);
        window_->showAt(QPoint(300, 300));
        QCoreApplication::processEvents();
        
        TEST_ASSERT(window_->isWindowVisible(), "输入状态时窗口应该可见");
        
        // 设置为非输入状态（即使有候选词）
        state.isComposing = false;
        window_->updateCandidates(state);
        QCoreApplication::processEvents();
        
        TEST_ASSERT(!window_->isWindowVisible(), "非输入状态时窗口应该隐藏");
        
        TEST_PASS("testNonComposingHidesWindow: 非输入状态正确隐藏窗口");
        return true;
    }

    /**
     * Property 5: 空候选词窗口自动隐藏
     * 
     * For any InputState，当 candidates 列表为空或 isComposing 为 false 时，
     * CandidateWindow 应处于隐藏状态。
     * 
     * Validates: Requirements 6.4
     */
    bool testProperty5_EmptyCandidatesAutoHide() {
        std::cout << "\n--- Property 5: 空候选词窗口自动隐藏 ---" << std::endl;
        std::cout << "  验证: 空候选词或非输入状态时窗口自动隐藏" << std::endl;
        
        const int NUM_ITERATIONS = 100;
        int testCount = 0;
        
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            InputState state;
            state.preedit = "test" + std::to_string(i);
            state.rawInput = state.preedit;
            state.highlightedIndex = 0;
            state.pageIndex = 0;
            state.pageSize = 9;
            state.hasMorePages = false;
            state.mode = InputMode::Chinese;
            
            // 随机决定是否有候选词
            bool hasCandidates = (std::rand() % 2) == 0;
            // 随机决定是否在输入状态
            bool isComposing = (std::rand() % 2) == 0;
            
            state.isComposing = isComposing;
            
            if (hasCandidates) {
                int numCandidates = (std::rand() % 9) + 1;
                for (int j = 0; j < numCandidates; j++) {
                    state.candidates.push_back({
                        "候选" + std::to_string(j),
                        "pinyin",
                        j + 1
                    });
                }
            }
            
            // 先显示窗口
            window_->showAt(QPoint(100 + (i % 10) * 50, 100 + (i / 10) * 50));
            window_->updateCandidates(state);
            QCoreApplication::processEvents();
            
            // 验证属性
            bool shouldBeVisible = !state.candidates.empty() && state.isComposing;
            bool actuallyVisible = window_->isWindowVisible();
            
            if (shouldBeVisible) {
                // 有候选词且在输入状态，窗口应该可见
                // 注意：这里不强制要求可见，因为 updateCandidates 的实现可能不会自动显示
            } else {
                // 空候选词或非输入状态，窗口应该隐藏
                TEST_ASSERT(!actuallyVisible,
                    "Property 5 违反: 空候选词或非输入状态时窗口应该隐藏 "
                    "(hasCandidates=" + std::to_string(hasCandidates) + 
                    ", isComposing=" + std::to_string(isComposing) + ")");
            }
            
            testCount++;
            
            // 清理
            window_->hideWindow();
            QCoreApplication::processEvents();
        }
        
        std::cout << "  完成 " << testCount << " 次随机测试" << std::endl;
        TEST_PASS("testProperty5_EmptyCandidatesAutoHide: 属性验证通过");
        return true;
    }

    CandidateWindow* window_;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    std::cout << "候选词窗口可见性测试程序启动" << std::endl;
    std::cout << "Qt Version: " << QT_VERSION_STR << std::endl;
    
    // 初始化 ConfigManager
    QString configDir = QDir::tempPath() + "/suyan_visibility_test";
    QDir().mkpath(configDir);
    
    if (!ConfigManager::instance().initialize(configDir.toStdString())) {
        std::cerr << "ConfigManager 初始化失败" << std::endl;
        return 1;
    }
    
    // 初始化 UI 组件
    UIInitConfig config;
    config.followSystemTheme = false;
    
    UIInitResult result = initializeUI(config);
    if (!result.success) {
        std::cerr << "UI 初始化失败: " << result.errorMessage.toStdString() << std::endl;
        return 1;
    }
    
    CandidateWindow* candidateWindow = result.window;
    candidateWindow->connectToThemeManager();
    candidateWindow->connectToLayoutManager();
    candidateWindow->syncFromManagers();
    
    // 运行测试
    CandidateWindowVisibilityTest test(candidateWindow);
    
    bool passed = false;
    QTimer::singleShot(100, [&]() {
        passed = test.runAllTests();
        cleanupUI(candidateWindow);
        QCoreApplication::exit(passed ? 0 : 1);
    });
    
    return app.exec();
}
