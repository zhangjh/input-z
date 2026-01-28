/**
 * ConfigManager - 配置管理器
 *
 * 负责读写 YAML 配置文件，提供配置访问接口，支持变更通知。
 * 使用单例模式管理全局配置。
 */

#ifndef SUYAN_CORE_CONFIG_MANAGER_H
#define SUYAN_CORE_CONFIG_MANAGER_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <optional>
#include <QObject>
#include <QString>

namespace suyan {

/**
 * 布局类型枚举
 */
enum class LayoutType {
    Horizontal,  // 横排
    Vertical     // 竖排
};

/**
 * 主题模式枚举
 */
enum class ThemeMode {
    Light,   // 浅色
    Dark,    // 深色
    Auto     // 跟随系统
};

/**
 * 输入默认模式枚举
 */
enum class DefaultInputMode {
    Chinese,  // 中文
    English   // 英文
};

/**
 * 布局配置结构
 */
struct LayoutConfig {
    LayoutType type = LayoutType::Horizontal;
    int pageSize = 9;
};

/**
 * 主题配置结构
 */
struct ThemeConfig {
    ThemeMode mode = ThemeMode::Auto;
    std::string customThemeName;  // 自定义主题名称（当 mode 不是 Auto 时使用）
};

/**
 * 输入配置结构
 */
struct InputConfig {
    DefaultInputMode defaultMode = DefaultInputMode::Chinese;
};

/**
 * 词频配置结构
 */
struct FrequencyConfig {
    bool enabled = true;
    int minCount = 3;  // 最小词频阈值
};

/**
 * 剪贴板配置结构
 */
struct ClipboardConfig {
    bool enabled = true;           // 是否启用剪贴板功能
    int maxAgeDays = 30;           // 保留天数（1-365）
    int maxCount = 1000;           // 最大保留条数（100-10000）
    std::string hotkey = "Cmd+Shift+V";  // 呼出快捷键
};

/**
 * 应用配置结构（汇总所有配置）
 */
struct AppConfig {
    LayoutConfig layout;
    ThemeConfig theme;
    InputConfig input;
    FrequencyConfig frequency;
    ClipboardConfig clipboard;
};

/**
 * 配置变更回调类型
 */
using ConfigChangedCallback = std::function<void(const std::string& key)>;

/**
 * ConfigManager - 配置管理器类
 *
 * 职责：
 * 1. 读写 YAML 配置文件
 * 2. 提供类型安全的配置访问接口
 * 3. 支持配置变更通知
 * 4. 配置持久化
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    /**
     * 获取单例实例
     */
    static ConfigManager& instance();

    // 禁止拷贝和移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    /**
     * 初始化配置管理器
     *
     * @param configDir 配置文件目录
     * @return 是否成功
     */
    bool initialize(const std::string& configDir);

    /**
     * 检查是否已初始化
     */
    bool isInitialized() const { return initialized_; }

    /**
     * 获取配置目录
     */
    std::string getConfigDir() const { return configDir_; }

    /**
     * 获取配置文件路径
     */
    std::string getConfigFilePath() const;

    // ========== 配置读取 ==========

    /**
     * 获取完整配置
     */
    AppConfig getConfig() const;

    /**
     * 获取布局配置
     */
    LayoutConfig getLayoutConfig() const;

    /**
     * 获取主题配置
     */
    ThemeConfig getThemeConfig() const;

    /**
     * 获取输入配置
     */
    InputConfig getInputConfig() const;

    /**
     * 获取词频配置
     */
    FrequencyConfig getFrequencyConfig() const;

    /**
     * 获取剪贴板配置
     */
    ClipboardConfig getClipboardConfig() const;

    // ========== 配置写入 ==========

    /**
     * 设置布局类型
     */
    void setLayoutType(LayoutType type);

    /**
     * 设置每页候选词数量
     */
    void setPageSize(int size);

    /**
     * 设置主题模式
     */
    void setThemeMode(ThemeMode mode);

    /**
     * 设置自定义主题名称
     */
    void setCustomThemeName(const std::string& name);

    /**
     * 设置默认输入模式
     */
    void setDefaultInputMode(DefaultInputMode mode);

    /**
     * 设置词频功能开关
     */
    void setFrequencyEnabled(bool enabled);

    /**
     * 设置最小词频阈值
     */
    void setFrequencyMinCount(int count);

    /**
     * 设置剪贴板功能开关
     */
    void setClipboardEnabled(bool enabled);

    /**
     * 设置剪贴板保留天数
     */
    void setClipboardMaxAgeDays(int days);

    /**
     * 设置剪贴板最大保留条数
     */
    void setClipboardMaxCount(int count);

    /**
     * 设置剪贴板呼出快捷键
     */
    void setClipboardHotkey(const std::string& hotkey);

    // ========== 通用配置访问 ==========

    /**
     * 获取字符串配置值
     *
     * @param key 配置键（支持点分隔的路径，如 "layout.type"）
     * @param defaultValue 默认值
     * @return 配置值
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * 获取整数配置值
     */
    int getInt(const std::string& key, int defaultValue = 0) const;

    /**
     * 获取布尔配置值
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;

    /**
     * 设置字符串配置值
     */
    void setString(const std::string& key, const std::string& value);

    /**
     * 设置整数配置值
     */
    void setInt(const std::string& key, int value);

    /**
     * 设置布尔配置值
     */
    void setBool(const std::string& key, bool value);

    // ========== 持久化 ==========

    /**
     * 保存配置到文件
     *
     * @return 是否成功
     */
    bool save();

    /**
     * 重新加载配置文件
     *
     * @return 是否成功
     */
    bool reload();

    /**
     * 重置为默认配置
     */
    void resetToDefaults();

signals:
    /**
     * 配置变更信号
     *
     * @param key 变更的配置键
     */
    void configChanged(const QString& key);

    /**
     * 布局配置变更信号
     */
    void layoutConfigChanged(const LayoutConfig& config);

    /**
     * 主题配置变更信号
     */
    void themeConfigChanged(const ThemeConfig& config);

    /**
     * 剪贴板配置变更信号
     */
    void clipboardConfigChanged(const ClipboardConfig& config);

private:
    ConfigManager();
    ~ConfigManager();

    // 内部方法
    bool loadConfig();
    bool saveConfig();
    void applyDefaults();
    void notifyChange(const std::string& key);

    // 成员变量
    bool initialized_ = false;
    std::string configDir_;
    AppConfig config_;
    
    // 配置文件名
    static constexpr const char* CONFIG_FILENAME = "config.yaml";
};

// ========== 辅助函数 ==========

/**
 * 布局类型转字符串
 */
std::string layoutTypeToString(LayoutType type);

/**
 * 字符串转布局类型
 */
LayoutType stringToLayoutType(const std::string& str);

/**
 * 主题模式转字符串
 */
std::string themeModeToString(ThemeMode mode);

/**
 * 字符串转主题模式
 */
ThemeMode stringToThemeMode(const std::string& str);

/**
 * 默认输入模式转字符串
 */
std::string defaultInputModeToString(DefaultInputMode mode);

/**
 * 字符串转默认输入模式
 */
DefaultInputMode stringToDefaultInputMode(const std::string& str);

} // namespace suyan

#endif // SUYAN_CORE_CONFIG_MANAGER_H
