/**
 * ConfigManager 实现
 */

#include "config_manager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace suyan {

// ========== 辅助函数实现 ==========

std::string layoutTypeToString(LayoutType type) {
    switch (type) {
        case LayoutType::Horizontal: return "horizontal";
        case LayoutType::Vertical: return "vertical";
        default: return "horizontal";
    }
}

LayoutType stringToLayoutType(const std::string& str) {
    if (str == "vertical") return LayoutType::Vertical;
    return LayoutType::Horizontal;  // 默认横排
}

std::string themeModeToString(ThemeMode mode) {
    switch (mode) {
        case ThemeMode::Light: return "light";
        case ThemeMode::Dark: return "dark";
        case ThemeMode::Auto: return "auto";
        default: return "auto";
    }
}

ThemeMode stringToThemeMode(const std::string& str) {
    if (str == "light") return ThemeMode::Light;
    if (str == "dark") return ThemeMode::Dark;
    return ThemeMode::Auto;  // 默认跟随系统
}

std::string defaultInputModeToString(DefaultInputMode mode) {
    switch (mode) {
        case DefaultInputMode::Chinese: return "chinese";
        case DefaultInputMode::English: return "english";
        default: return "chinese";
    }
}

DefaultInputMode stringToDefaultInputMode(const std::string& str) {
    if (str == "english") return DefaultInputMode::English;
    return DefaultInputMode::Chinese;  // 默认中文
}

// ========== ConfigManager 实现 ==========

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() : QObject(nullptr) {
    applyDefaults();
}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::initialize(const std::string& configDir) {
    if (initialized_) {
        return true;  // 已初始化，幂等
    }

    configDir_ = configDir;

    // 确保配置目录存在
    try {
        fs::create_directories(configDir_);
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: 创建配置目录失败: " << e.what() << std::endl;
        return false;
    }

    // 加载配置文件
    if (!loadConfig()) {
        // 配置文件不存在或加载失败，使用默认配置并保存
        applyDefaults();
        saveConfig();
    }

    initialized_ = true;
    return true;
}

std::string ConfigManager::getConfigFilePath() const {
    return configDir_ + "/" + CONFIG_FILENAME;
}

bool ConfigManager::loadConfig() {
    std::string configPath = getConfigFilePath();
    
    if (!fs::exists(configPath)) {
        return false;
    }

    try {
        YAML::Node root = YAML::LoadFile(configPath);

        // 读取布局配置
        if (root["layout"]) {
            auto layout = root["layout"];
            if (layout["type"]) {
                config_.layout.type = stringToLayoutType(layout["type"].as<std::string>());
            }
            if (layout["page_size"]) {
                config_.layout.pageSize = layout["page_size"].as<int>();
            }
        }

        // 读取主题配置
        if (root["theme"]) {
            auto theme = root["theme"];
            if (theme["mode"]) {
                config_.theme.mode = stringToThemeMode(theme["mode"].as<std::string>());
            }
            if (theme["custom_name"]) {
                config_.theme.customThemeName = theme["custom_name"].as<std::string>();
            }
        }

        // 读取输入配置
        if (root["input"]) {
            auto input = root["input"];
            if (input["default_mode"]) {
                config_.input.defaultMode = stringToDefaultInputMode(input["default_mode"].as<std::string>());
            }
        }

        // 读取词频配置
        if (root["frequency"]) {
            auto freq = root["frequency"];
            if (freq["enabled"]) {
                config_.frequency.enabled = freq["enabled"].as<bool>();
            }
            if (freq["min_count"]) {
                config_.frequency.minCount = freq["min_count"].as<int>();
            }
        }

        // 读取剪贴板配置
        if (root["clipboard"]) {
            auto clipboard = root["clipboard"];
            if (clipboard["enabled"]) {
                config_.clipboard.enabled = clipboard["enabled"].as<bool>();
            }
            if (clipboard["max_age_days"]) {
                config_.clipboard.maxAgeDays = clipboard["max_age_days"].as<int>();
            }
            if (clipboard["max_count"]) {
                config_.clipboard.maxCount = clipboard["max_count"].as<int>();
            }
            if (clipboard["hotkey"]) {
                config_.clipboard.hotkey = clipboard["hotkey"].as<std::string>();
            }
        }

        return true;
    } catch (const YAML::Exception& e) {
        std::cerr << "ConfigManager: YAML 解析错误: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: 加载配置失败: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveConfig() {
    std::string configPath = getConfigFilePath();

    try {
        YAML::Emitter out;
        out << YAML::BeginMap;

        // 写入布局配置
        out << YAML::Key << "layout" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "type" << YAML::Value << layoutTypeToString(config_.layout.type);
        out << YAML::Key << "page_size" << YAML::Value << config_.layout.pageSize;
        out << YAML::EndMap;

        // 写入主题配置
        out << YAML::Key << "theme" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "mode" << YAML::Value << themeModeToString(config_.theme.mode);
        if (!config_.theme.customThemeName.empty()) {
            out << YAML::Key << "custom_name" << YAML::Value << config_.theme.customThemeName;
        }
        out << YAML::EndMap;

        // 写入输入配置
        out << YAML::Key << "input" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "default_mode" << YAML::Value << defaultInputModeToString(config_.input.defaultMode);
        out << YAML::EndMap;

        // 写入词频配置
        out << YAML::Key << "frequency" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "enabled" << YAML::Value << config_.frequency.enabled;
        out << YAML::Key << "min_count" << YAML::Value << config_.frequency.minCount;
        out << YAML::EndMap;

        // 写入剪贴板配置
        out << YAML::Key << "clipboard" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "enabled" << YAML::Value << config_.clipboard.enabled;
        out << YAML::Key << "max_age_days" << YAML::Value << config_.clipboard.maxAgeDays;
        out << YAML::Key << "max_count" << YAML::Value << config_.clipboard.maxCount;
        out << YAML::Key << "hotkey" << YAML::Value << config_.clipboard.hotkey;
        out << YAML::EndMap;

        out << YAML::EndMap;

        // 写入文件
        std::ofstream fout(configPath);
        if (!fout.is_open()) {
            std::cerr << "ConfigManager: 无法打开配置文件进行写入: " << configPath << std::endl;
            return false;
        }
        fout << "# SuYan 输入法配置文件\n\n";
        fout << out.c_str();
        fout.close();

        return true;
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: 保存配置失败: " << e.what() << std::endl;
        return false;
    }
}

void ConfigManager::applyDefaults() {
    config_ = AppConfig{};  // 使用结构体默认值
}

void ConfigManager::notifyChange(const std::string& key) {
    emit configChanged(QString::fromStdString(key));

    // 根据 key 发送特定信号
    if (key.find("layout") == 0) {
        emit layoutConfigChanged(config_.layout);
    } else if (key.find("theme") == 0) {
        emit themeConfigChanged(config_.theme);
    } else if (key.find("clipboard") == 0) {
        emit clipboardConfigChanged(config_.clipboard);
    }
}

// ========== 配置读取 ==========

AppConfig ConfigManager::getConfig() const {
    return config_;
}

LayoutConfig ConfigManager::getLayoutConfig() const {
    return config_.layout;
}

ThemeConfig ConfigManager::getThemeConfig() const {
    return config_.theme;
}

InputConfig ConfigManager::getInputConfig() const {
    return config_.input;
}

FrequencyConfig ConfigManager::getFrequencyConfig() const {
    return config_.frequency;
}

ClipboardConfig ConfigManager::getClipboardConfig() const {
    return config_.clipboard;
}

// ========== 配置写入 ==========

void ConfigManager::setLayoutType(LayoutType type) {
    if (config_.layout.type != type) {
        config_.layout.type = type;
        notifyChange("layout.type");
    }
}

void ConfigManager::setPageSize(int size) {
    if (size < 1) size = 1;
    if (size > 10) size = 10;
    
    if (config_.layout.pageSize != size) {
        config_.layout.pageSize = size;
        notifyChange("layout.page_size");
    }
}

void ConfigManager::setThemeMode(ThemeMode mode) {
    if (config_.theme.mode != mode) {
        config_.theme.mode = mode;
        notifyChange("theme.mode");
    }
}

void ConfigManager::setCustomThemeName(const std::string& name) {
    if (config_.theme.customThemeName != name) {
        config_.theme.customThemeName = name;
        notifyChange("theme.custom_name");
    }
}

void ConfigManager::setDefaultInputMode(DefaultInputMode mode) {
    if (config_.input.defaultMode != mode) {
        config_.input.defaultMode = mode;
        notifyChange("input.default_mode");
    }
}

void ConfigManager::setFrequencyEnabled(bool enabled) {
    if (config_.frequency.enabled != enabled) {
        config_.frequency.enabled = enabled;
        notifyChange("frequency.enabled");
    }
}

void ConfigManager::setFrequencyMinCount(int count) {
    if (count < 1) count = 1;
    
    if (config_.frequency.minCount != count) {
        config_.frequency.minCount = count;
        notifyChange("frequency.min_count");
    }
}

void ConfigManager::setClipboardEnabled(bool enabled) {
    if (config_.clipboard.enabled != enabled) {
        config_.clipboard.enabled = enabled;
        notifyChange("clipboard.enabled");
    }
}

void ConfigManager::setClipboardMaxAgeDays(int days) {
    if (days < 1) days = 1;
    if (days > 365) days = 365;
    
    if (config_.clipboard.maxAgeDays != days) {
        config_.clipboard.maxAgeDays = days;
        notifyChange("clipboard.max_age_days");
    }
}

void ConfigManager::setClipboardMaxCount(int count) {
    if (count < 100) count = 100;
    if (count > 10000) count = 10000;
    
    if (config_.clipboard.maxCount != count) {
        config_.clipboard.maxCount = count;
        notifyChange("clipboard.max_count");
    }
}

void ConfigManager::setClipboardHotkey(const std::string& hotkey) {
    if (config_.clipboard.hotkey != hotkey) {
        config_.clipboard.hotkey = hotkey;
        notifyChange("clipboard.hotkey");
    }
}

// ========== 通用配置访问 ==========

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    // 解析点分隔的路径
    if (key == "layout.type") {
        return layoutTypeToString(config_.layout.type);
    } else if (key == "theme.mode") {
        return themeModeToString(config_.theme.mode);
    } else if (key == "theme.custom_name") {
        return config_.theme.customThemeName;
    } else if (key == "input.default_mode") {
        return defaultInputModeToString(config_.input.defaultMode);
    } else if (key == "clipboard.hotkey") {
        return config_.clipboard.hotkey;
    }
    return defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    if (key == "layout.page_size") {
        return config_.layout.pageSize;
    } else if (key == "frequency.min_count") {
        return config_.frequency.minCount;
    } else if (key == "clipboard.max_age_days") {
        return config_.clipboard.maxAgeDays;
    } else if (key == "clipboard.max_count") {
        return config_.clipboard.maxCount;
    }
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    if (key == "frequency.enabled") {
        return config_.frequency.enabled;
    } else if (key == "clipboard.enabled") {
        return config_.clipboard.enabled;
    }
    return defaultValue;
}

void ConfigManager::setString(const std::string& key, const std::string& value) {
    if (key == "layout.type") {
        setLayoutType(stringToLayoutType(value));
    } else if (key == "theme.mode") {
        setThemeMode(stringToThemeMode(value));
    } else if (key == "theme.custom_name") {
        setCustomThemeName(value);
    } else if (key == "input.default_mode") {
        setDefaultInputMode(stringToDefaultInputMode(value));
    } else if (key == "clipboard.hotkey") {
        setClipboardHotkey(value);
    }
}

void ConfigManager::setInt(const std::string& key, int value) {
    if (key == "layout.page_size") {
        setPageSize(value);
    } else if (key == "frequency.min_count") {
        setFrequencyMinCount(value);
    } else if (key == "clipboard.max_age_days") {
        setClipboardMaxAgeDays(value);
    } else if (key == "clipboard.max_count") {
        setClipboardMaxCount(value);
    }
}

void ConfigManager::setBool(const std::string& key, bool value) {
    if (key == "frequency.enabled") {
        setFrequencyEnabled(value);
    } else if (key == "clipboard.enabled") {
        setClipboardEnabled(value);
    }
}

// ========== 持久化 ==========

bool ConfigManager::save() {
    return saveConfig();
}

bool ConfigManager::reload() {
    if (!loadConfig()) {
        return false;
    }
    // 通知所有配置已变更
    notifyChange("*");
    return true;
}

void ConfigManager::resetToDefaults() {
    applyDefaults();
    notifyChange("*");
}

} // namespace suyan
