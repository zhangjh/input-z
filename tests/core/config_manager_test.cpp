/**
 * ConfigManager 单元测试
 *
 * 测试配置管理器的 YAML 读写、配置访问和变更通知功能。
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <QCoreApplication>
#include <QSignalSpy>
#include "config_manager.h"

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

class ConfigManagerTest {
public:
    ConfigManagerTest() {
        // 使用临时目录进行测试
        testConfigDir_ = fs::temp_directory_path().string() + "/suyan_config_test";
        
        // 清理之前的测试数据
        fs::remove_all(testConfigDir_);
    }
    
    ~ConfigManagerTest() {
        // 清理测试数据
        fs::remove_all(testConfigDir_);
    }
    
    bool runAllTests() {
        std::cout << "=== ConfigManager 单元测试 ===" << std::endl;
        std::cout << "测试配置目录: " << testConfigDir_ << std::endl;
        std::cout << std::endl;
        
        bool allPassed = true;
        
        // 基础测试
        allPassed &= testGetInstance();
        allPassed &= testInitialize();
        
        // 配置读写测试
        allPassed &= testDefaultConfig();
        allPassed &= testSetLayoutType();
        allPassed &= testSetPageSize();
        allPassed &= testSetThemeMode();
        allPassed &= testSetDefaultInputMode();
        allPassed &= testSetFrequencyConfig();
        
        // 通用访问测试
        allPassed &= testGenericAccess();
        
        // 持久化测试
        allPassed &= testSaveAndReload();
        
        // 重置测试
        allPassed &= testResetToDefaults();
        
        // 剪贴板配置测试
        allPassed &= testClipboardConfig();
        allPassed &= testClipboardConfigPersistence();
        
        // 信号测试
        allPassed &= testSignals();
        
        // 辅助函数测试
        allPassed &= testHelperFunctions();
        
        std::cout << std::endl;
        if (allPassed) {
            std::cout << "=== 所有测试通过 ===" << std::endl;
        } else {
            std::cout << "=== 部分测试失败 ===" << std::endl;
        }
        
        return allPassed;
    }
    
private:
    std::string testConfigDir_;
    
    // ========== 基础测试 ==========
    
    bool testGetInstance() {
        auto& config1 = suyan::ConfigManager::instance();
        auto& config2 = suyan::ConfigManager::instance();
        TEST_ASSERT(&config1 == &config2, "单例实例应该相同");
        TEST_PASS("testGetInstance: 单例模式正常");
        return true;
    }
    
    bool testInitialize() {
        auto& config = suyan::ConfigManager::instance();
        
        // 首次初始化
        bool result = config.initialize(testConfigDir_);
        TEST_ASSERT(result, "初始化应该成功");
        TEST_ASSERT(config.isInitialized(), "初始化后 isInitialized 应该返回 true");
        TEST_ASSERT(config.getConfigDir() == testConfigDir_, "配置目录应该正确");
        
        // 检查配置目录已创建
        TEST_ASSERT(fs::exists(testConfigDir_), "配置目录应该已创建");
        
        // 重复初始化应该返回 true（幂等）
        result = config.initialize(testConfigDir_);
        TEST_ASSERT(result, "重复初始化应该返回 true");
        
        TEST_PASS("testInitialize: 初始化成功");
        return true;
    }
    
    // ========== 配置读写测试 ==========
    
    bool testDefaultConfig() {
        auto& config = suyan::ConfigManager::instance();
        
        // 检查默认值
        auto layoutConfig = config.getLayoutConfig();
        TEST_ASSERT(layoutConfig.type == suyan::LayoutType::Horizontal, "默认布局应该是横排");
        TEST_ASSERT(layoutConfig.pageSize == 9, "默认每页候选词数量应该是 9");
        
        auto themeConfig = config.getThemeConfig();
        TEST_ASSERT(themeConfig.mode == suyan::ThemeMode::Auto, "默认主题模式应该是跟随系统");
        
        auto inputConfig = config.getInputConfig();
        TEST_ASSERT(inputConfig.defaultMode == suyan::DefaultInputMode::Chinese, "默认输入模式应该是中文");
        
        auto freqConfig = config.getFrequencyConfig();
        TEST_ASSERT(freqConfig.enabled == true, "词频功能默认应该启用");
        TEST_ASSERT(freqConfig.minCount == 3, "默认最小词频阈值应该是 3");
        
        TEST_PASS("testDefaultConfig: 默认配置正确");
        return true;
    }
    
    bool testSetLayoutType() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置为竖排
        config.setLayoutType(suyan::LayoutType::Vertical);
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Vertical, "布局类型应该是竖排");
        
        // 设置回横排
        config.setLayoutType(suyan::LayoutType::Horizontal);
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Horizontal, "布局类型应该是横排");
        
        TEST_PASS("testSetLayoutType: 设置布局类型正常");
        return true;
    }
    
    bool testSetPageSize() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置有效值
        config.setPageSize(5);
        TEST_ASSERT(config.getLayoutConfig().pageSize == 5, "每页候选词数量应该是 5");
        
        // 测试边界值
        config.setPageSize(0);  // 应该被限制为 1
        TEST_ASSERT(config.getLayoutConfig().pageSize == 1, "每页候选词数量应该被限制为 1");
        
        config.setPageSize(15);  // 应该被限制为 10
        TEST_ASSERT(config.getLayoutConfig().pageSize == 10, "每页候选词数量应该被限制为 10");
        
        // 恢复默认值
        config.setPageSize(9);
        
        TEST_PASS("testSetPageSize: 设置每页候选词数量正常");
        return true;
    }
    
    bool testSetThemeMode() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置为浅色
        config.setThemeMode(suyan::ThemeMode::Light);
        TEST_ASSERT(config.getThemeConfig().mode == suyan::ThemeMode::Light, "主题模式应该是浅色");
        
        // 设置为深色
        config.setThemeMode(suyan::ThemeMode::Dark);
        TEST_ASSERT(config.getThemeConfig().mode == suyan::ThemeMode::Dark, "主题模式应该是深色");
        
        // 设置为跟随系统
        config.setThemeMode(suyan::ThemeMode::Auto);
        TEST_ASSERT(config.getThemeConfig().mode == suyan::ThemeMode::Auto, "主题模式应该是跟随系统");
        
        // 测试自定义主题名称
        config.setCustomThemeName("my_theme");
        TEST_ASSERT(config.getThemeConfig().customThemeName == "my_theme", "自定义主题名称应该正确");
        
        config.setCustomThemeName("");  // 清空
        
        TEST_PASS("testSetThemeMode: 设置主题模式正常");
        return true;
    }
    
    bool testSetDefaultInputMode() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置为英文
        config.setDefaultInputMode(suyan::DefaultInputMode::English);
        TEST_ASSERT(config.getInputConfig().defaultMode == suyan::DefaultInputMode::English, "默认输入模式应该是英文");
        
        // 设置回中文
        config.setDefaultInputMode(suyan::DefaultInputMode::Chinese);
        TEST_ASSERT(config.getInputConfig().defaultMode == suyan::DefaultInputMode::Chinese, "默认输入模式应该是中文");
        
        TEST_PASS("testSetDefaultInputMode: 设置默认输入模式正常");
        return true;
    }
    
    bool testSetFrequencyConfig() {
        auto& config = suyan::ConfigManager::instance();
        
        // 禁用词频
        config.setFrequencyEnabled(false);
        TEST_ASSERT(config.getFrequencyConfig().enabled == false, "词频功能应该被禁用");
        
        // 启用词频
        config.setFrequencyEnabled(true);
        TEST_ASSERT(config.getFrequencyConfig().enabled == true, "词频功能应该被启用");
        
        // 设置最小词频阈值
        config.setFrequencyMinCount(5);
        TEST_ASSERT(config.getFrequencyConfig().minCount == 5, "最小词频阈值应该是 5");
        
        // 测试边界值
        config.setFrequencyMinCount(0);  // 应该被限制为 1
        TEST_ASSERT(config.getFrequencyConfig().minCount == 1, "最小词频阈值应该被限制为 1");
        
        // 恢复默认值
        config.setFrequencyMinCount(3);
        
        TEST_PASS("testSetFrequencyConfig: 设置词频配置正常");
        return true;
    }
    
    // ========== 通用访问测试 ==========
    
    bool testGenericAccess() {
        auto& config = suyan::ConfigManager::instance();
        
        // 测试字符串访问
        config.setString("layout.type", "vertical");
        TEST_ASSERT(config.getString("layout.type") == "vertical", "通过字符串设置布局类型应该生效");
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Vertical, "布局类型应该是竖排");
        
        config.setString("layout.type", "horizontal");
        
        // 测试整数访问
        config.setInt("layout.page_size", 7);
        TEST_ASSERT(config.getInt("layout.page_size") == 7, "通过整数设置每页候选词数量应该生效");
        
        config.setInt("layout.page_size", 9);
        
        // 测试布尔访问
        config.setBool("frequency.enabled", false);
        TEST_ASSERT(config.getBool("frequency.enabled") == false, "通过布尔设置词频开关应该生效");
        
        config.setBool("frequency.enabled", true);
        
        // 测试默认值
        TEST_ASSERT(config.getString("nonexistent.key", "default") == "default", "不存在的键应该返回默认值");
        TEST_ASSERT(config.getInt("nonexistent.key", 42) == 42, "不存在的键应该返回默认值");
        TEST_ASSERT(config.getBool("nonexistent.key", true) == true, "不存在的键应该返回默认值");
        
        TEST_PASS("testGenericAccess: 通用配置访问正常");
        return true;
    }
    
    // ========== 持久化测试 ==========
    
    bool testSaveAndReload() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置一些配置
        config.setLayoutType(suyan::LayoutType::Vertical);
        config.setPageSize(7);
        config.setThemeMode(suyan::ThemeMode::Dark);
        config.setDefaultInputMode(suyan::DefaultInputMode::English);
        config.setFrequencyEnabled(false);
        config.setFrequencyMinCount(5);
        
        // 保存
        bool saveResult = config.save();
        TEST_ASSERT(saveResult, "保存配置应该成功");
        
        // 检查配置文件存在
        std::string configPath = config.getConfigFilePath();
        TEST_ASSERT(fs::exists(configPath), "配置文件应该存在");
        
        // 打印配置文件内容
        std::cout << "  配置文件路径: " << configPath << std::endl;
        std::ifstream configFile(configPath);
        if (configFile.is_open()) {
            std::cout << "  配置文件内容:" << std::endl;
            std::string line;
            while (std::getline(configFile, line)) {
                std::cout << "    " << line << std::endl;
            }
            configFile.close();
        }
        
        // 重置为默认值
        config.resetToDefaults();
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Horizontal, "重置后布局应该是横排");
        
        // 重新加载
        bool reloadResult = config.reload();
        TEST_ASSERT(reloadResult, "重新加载配置应该成功");
        
        // 验证配置已恢复
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Vertical, "重新加载后布局应该是竖排");
        TEST_ASSERT(config.getLayoutConfig().pageSize == 7, "重新加载后每页候选词数量应该是 7");
        TEST_ASSERT(config.getThemeConfig().mode == suyan::ThemeMode::Dark, "重新加载后主题模式应该是深色");
        TEST_ASSERT(config.getInputConfig().defaultMode == suyan::DefaultInputMode::English, "重新加载后默认输入模式应该是英文");
        TEST_ASSERT(config.getFrequencyConfig().enabled == false, "重新加载后词频功能应该被禁用");
        TEST_ASSERT(config.getFrequencyConfig().minCount == 5, "重新加载后最小词频阈值应该是 5");
        
        // 恢复默认值以便后续测试
        config.resetToDefaults();
        config.save();
        
        TEST_PASS("testSaveAndReload: 保存和重新加载正常");
        return true;
    }
    
    // ========== 重置测试 ==========
    
    bool testResetToDefaults() {
        auto& config = suyan::ConfigManager::instance();
        
        // 修改配置
        config.setLayoutType(suyan::LayoutType::Vertical);
        config.setThemeMode(suyan::ThemeMode::Dark);
        
        // 重置
        config.resetToDefaults();
        
        // 验证已重置
        TEST_ASSERT(config.getLayoutConfig().type == suyan::LayoutType::Horizontal, "重置后布局应该是横排");
        TEST_ASSERT(config.getThemeConfig().mode == suyan::ThemeMode::Auto, "重置后主题模式应该是跟随系统");
        
        TEST_PASS("testResetToDefaults: 重置为默认配置正常");
        return true;
    }
    
    // ========== 剪贴板配置测试 ==========
    
    bool testClipboardConfig() {
        auto& config = suyan::ConfigManager::instance();
        
        // 检查默认值
        auto clipboardConfig = config.getClipboardConfig();
        TEST_ASSERT(clipboardConfig.enabled == true, "剪贴板功能默认应该启用");
        TEST_ASSERT(clipboardConfig.maxAgeDays == 30, "默认保留天数应该是 30");
        TEST_ASSERT(clipboardConfig.maxCount == 1000, "默认最大条数应该是 1000");
        TEST_ASSERT(clipboardConfig.hotkey == "Cmd+Shift+V", "默认快捷键应该是 Cmd+Shift+V");
        
        // 测试启用/禁用
        config.setClipboardEnabled(false);
        TEST_ASSERT(config.getClipboardConfig().enabled == false, "剪贴板功能应该被禁用");
        config.setClipboardEnabled(true);
        TEST_ASSERT(config.getClipboardConfig().enabled == true, "剪贴板功能应该被启用");
        
        // 测试保留天数
        config.setClipboardMaxAgeDays(7);
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 7, "保留天数应该是 7");
        
        // 测试边界值
        config.setClipboardMaxAgeDays(0);  // 应该被限制为 1
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 1, "保留天数应该被限制为 1");
        config.setClipboardMaxAgeDays(500);  // 应该被限制为 365
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 365, "保留天数应该被限制为 365");
        
        // 测试最大条数
        config.setClipboardMaxCount(500);
        TEST_ASSERT(config.getClipboardConfig().maxCount == 500, "最大条数应该是 500");
        
        // 测试边界值
        config.setClipboardMaxCount(50);  // 应该被限制为 100
        TEST_ASSERT(config.getClipboardConfig().maxCount == 100, "最大条数应该被限制为 100");
        config.setClipboardMaxCount(20000);  // 应该被限制为 10000
        TEST_ASSERT(config.getClipboardConfig().maxCount == 10000, "最大条数应该被限制为 10000");
        
        // 测试快捷键
        config.setClipboardHotkey("Ctrl+Shift+C");
        TEST_ASSERT(config.getClipboardConfig().hotkey == "Ctrl+Shift+C", "快捷键应该是 Ctrl+Shift+C");
        
        // 测试通用访问
        TEST_ASSERT(config.getBool("clipboard.enabled") == true, "通过通用访问获取启用状态");
        TEST_ASSERT(config.getInt("clipboard.max_age_days") == 365, "通过通用访问获取保留天数");
        TEST_ASSERT(config.getInt("clipboard.max_count") == 10000, "通过通用访问获取最大条数");
        TEST_ASSERT(config.getString("clipboard.hotkey") == "Ctrl+Shift+C", "通过通用访问获取快捷键");
        
        // 测试通用设置
        config.setBool("clipboard.enabled", false);
        TEST_ASSERT(config.getClipboardConfig().enabled == false, "通过通用访问设置启用状态");
        config.setInt("clipboard.max_age_days", 14);
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 14, "通过通用访问设置保留天数");
        config.setInt("clipboard.max_count", 2000);
        TEST_ASSERT(config.getClipboardConfig().maxCount == 2000, "通过通用访问设置最大条数");
        config.setString("clipboard.hotkey", "Alt+V");
        TEST_ASSERT(config.getClipboardConfig().hotkey == "Alt+V", "通过通用访问设置快捷键");
        
        // 恢复默认值
        config.resetToDefaults();
        
        TEST_PASS("testClipboardConfig: 剪贴板配置读写正常");
        return true;
    }
    
    bool testClipboardConfigPersistence() {
        auto& config = suyan::ConfigManager::instance();
        
        // 设置剪贴板配置
        config.setClipboardEnabled(false);
        config.setClipboardMaxAgeDays(7);
        config.setClipboardMaxCount(500);
        config.setClipboardHotkey("Cmd+Alt+V");
        
        // 保存
        bool saveResult = config.save();
        TEST_ASSERT(saveResult, "保存配置应该成功");
        
        // 重置为默认值
        config.resetToDefaults();
        TEST_ASSERT(config.getClipboardConfig().enabled == true, "重置后剪贴板应该启用");
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 30, "重置后保留天数应该是 30");
        
        // 重新加载
        bool reloadResult = config.reload();
        TEST_ASSERT(reloadResult, "重新加载配置应该成功");
        
        // 验证配置已恢复
        TEST_ASSERT(config.getClipboardConfig().enabled == false, "重新加载后剪贴板应该禁用");
        TEST_ASSERT(config.getClipboardConfig().maxAgeDays == 7, "重新加载后保留天数应该是 7");
        TEST_ASSERT(config.getClipboardConfig().maxCount == 500, "重新加载后最大条数应该是 500");
        TEST_ASSERT(config.getClipboardConfig().hotkey == "Cmd+Alt+V", "重新加载后快捷键应该是 Cmd+Alt+V");
        
        // 恢复默认值以便后续测试
        config.resetToDefaults();
        config.save();
        
        TEST_PASS("testClipboardConfigPersistence: 剪贴板配置持久化正常");
        return true;
    }
    
    // ========== 信号测试 ==========
    
    bool testSignals() {
        auto& config = suyan::ConfigManager::instance();
        
        // 使用 QSignalSpy 监听信号
        QSignalSpy configChangedSpy(&config, &suyan::ConfigManager::configChanged);
        QSignalSpy layoutChangedSpy(&config, &suyan::ConfigManager::layoutConfigChanged);
        QSignalSpy themeChangedSpy(&config, &suyan::ConfigManager::themeConfigChanged);
        QSignalSpy clipboardChangedSpy(&config, &suyan::ConfigManager::clipboardConfigChanged);
        
        // 修改布局配置
        config.setLayoutType(suyan::LayoutType::Vertical);
        
        TEST_ASSERT(configChangedSpy.count() >= 1, "configChanged 信号应该被发送");
        TEST_ASSERT(layoutChangedSpy.count() >= 1, "layoutConfigChanged 信号应该被发送");
        
        // 修改主题配置
        config.setThemeMode(suyan::ThemeMode::Dark);
        
        TEST_ASSERT(themeChangedSpy.count() >= 1, "themeConfigChanged 信号应该被发送");
        
        // 修改剪贴板配置
        config.setClipboardEnabled(false);
        
        TEST_ASSERT(clipboardChangedSpy.count() >= 1, "clipboardConfigChanged 信号应该被发送");
        
        // 恢复默认值
        config.resetToDefaults();
        
        TEST_PASS("testSignals: 信号发送正常");
        return true;
    }
    
    // ========== 辅助函数测试 ==========
    
    bool testHelperFunctions() {
        // 测试布局类型转换
        TEST_ASSERT(suyan::layoutTypeToString(suyan::LayoutType::Horizontal) == "horizontal", "横排转字符串");
        TEST_ASSERT(suyan::layoutTypeToString(suyan::LayoutType::Vertical) == "vertical", "竖排转字符串");
        TEST_ASSERT(suyan::stringToLayoutType("horizontal") == suyan::LayoutType::Horizontal, "字符串转横排");
        TEST_ASSERT(suyan::stringToLayoutType("vertical") == suyan::LayoutType::Vertical, "字符串转竖排");
        TEST_ASSERT(suyan::stringToLayoutType("invalid") == suyan::LayoutType::Horizontal, "无效字符串默认横排");
        
        // 测试主题模式转换
        TEST_ASSERT(suyan::themeModeToString(suyan::ThemeMode::Light) == "light", "浅色转字符串");
        TEST_ASSERT(suyan::themeModeToString(suyan::ThemeMode::Dark) == "dark", "深色转字符串");
        TEST_ASSERT(suyan::themeModeToString(suyan::ThemeMode::Auto) == "auto", "跟随系统转字符串");
        TEST_ASSERT(suyan::stringToThemeMode("light") == suyan::ThemeMode::Light, "字符串转浅色");
        TEST_ASSERT(suyan::stringToThemeMode("dark") == suyan::ThemeMode::Dark, "字符串转深色");
        TEST_ASSERT(suyan::stringToThemeMode("auto") == suyan::ThemeMode::Auto, "字符串转跟随系统");
        TEST_ASSERT(suyan::stringToThemeMode("invalid") == suyan::ThemeMode::Auto, "无效字符串默认跟随系统");
        
        // 测试默认输入模式转换
        TEST_ASSERT(suyan::defaultInputModeToString(suyan::DefaultInputMode::Chinese) == "chinese", "中文转字符串");
        TEST_ASSERT(suyan::defaultInputModeToString(suyan::DefaultInputMode::English) == "english", "英文转字符串");
        TEST_ASSERT(suyan::stringToDefaultInputMode("chinese") == suyan::DefaultInputMode::Chinese, "字符串转中文");
        TEST_ASSERT(suyan::stringToDefaultInputMode("english") == suyan::DefaultInputMode::English, "字符串转英文");
        TEST_ASSERT(suyan::stringToDefaultInputMode("invalid") == suyan::DefaultInputMode::Chinese, "无效字符串默认中文");
        
        TEST_PASS("testHelperFunctions: 辅助函数正常");
        return true;
    }
};

int main(int argc, char* argv[]) {
    // 需要 QCoreApplication 来支持 Qt 信号
    QCoreApplication app(argc, argv);
    
    ConfigManagerTest test;
    return test.runAllTests() ? 0 : 1;
}
