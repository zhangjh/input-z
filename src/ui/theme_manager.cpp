/**
 * ThemeManager 实现
 */

#include "theme_manager.h"
#include "../core/config_manager.h"
#include <yaml-cpp/yaml.h>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <iostream>

namespace suyan {

// 常量定义
constexpr const char* Theme::NAME_LIGHT;
constexpr const char* Theme::NAME_DARK;
constexpr const char* ThemeManager::THEME_LIGHT;
constexpr const char* ThemeManager::THEME_DARK;
constexpr const char* ThemeManager::THEME_AUTO;

// ========== Theme 静态方法实现 ==========

Theme Theme::defaultLight() {
    Theme theme;
    theme.name = NAME_LIGHT;
    
    // 背景
    theme.backgroundColor = QColor("#FFFFFF");
    theme.backgroundOpacity = 95;
    theme.borderRadius = 8;
    theme.borderColor = QColor("#E0E0E0");
    theme.borderWidth = 1;
    
    // 文字
    theme.fontFamily = "PingFang SC";
    theme.fontSize = 16;
    theme.textColor = QColor("#333333");
    theme.highlightTextColor = QColor("#FFFFFF");
    theme.highlightBackColor = QColor("#007AFF");
    theme.preeditColor = QColor("#666666");
    theme.labelColor = QColor("#999999");
    theme.commentColor = QColor("#999999");
    
    // 间距
    theme.candidateSpacing = 8;
    theme.padding = 4;
    
    return theme;
}

Theme Theme::defaultDark() {
    Theme theme;
    theme.name = NAME_DARK;
    
    // 背景
    theme.backgroundColor = QColor("#2D2D2D");
    theme.backgroundOpacity = 95;
    theme.borderRadius = 8;
    theme.borderColor = QColor("#404040");
    theme.borderWidth = 1;
    
    // 文字
    theme.fontFamily = "PingFang SC";
    theme.fontSize = 16;
    theme.textColor = QColor("#E0E0E0");
    theme.highlightTextColor = QColor("#FFFFFF");
    theme.highlightBackColor = QColor("#0A84FF");
    theme.preeditColor = QColor("#A0A0A0");
    theme.labelColor = QColor("#808080");
    theme.commentColor = QColor("#808080");
    
    // 间距
    theme.candidateSpacing = 8;
    theme.padding = 4;
    
    return theme;
}

// ========== ThemeManager 实现 ==========

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() : QObject(nullptr) {
    // 加载内置主题
    loadBuiltinThemes();
    
    // 默认跟随系统
    followSystem_ = true;
    currentThemeName_ = THEME_AUTO;
}

ThemeManager::~ThemeManager() = default;

bool ThemeManager::initialize(const QString& themesDir) {
    if (initialized_) {
        return true;
    }
    
    themesDir_ = themesDir;
    
    // 如果指定了主题目录，加载自定义主题
    if (!themesDir.isEmpty()) {
        loadThemesFromDirectory(themesDir);
    }
    
    // 设置系统主题监听
    setupSystemThemeMonitor();
    
    // 设置 ConfigManager 连接
    setupConfigManagerConnection();
    
    // 初始化系统深色模式状态
    cachedSystemDarkMode_ = isSystemDarkMode();
    
    // 从 ConfigManager 同步主题配置
    syncFromConfigManager();
    
    initialized_ = true;
    return true;
}

void ThemeManager::loadBuiltinThemes() {
    // 添加内置浅色主题
    themes_[THEME_LIGHT] = Theme::defaultLight();
    
    // 添加内置深色主题
    themes_[THEME_DARK] = Theme::defaultDark();
}

bool ThemeManager::loadThemeFromFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        std::cerr << "ThemeManager: 主题文件不存在: " 
                  << filePath.toStdString() << std::endl;
        return false;
    }
    
    Theme theme = parseThemeYaml(filePath);
    if (!theme.isValid()) {
        std::cerr << "ThemeManager: 主题文件解析失败: " 
                  << filePath.toStdString() << std::endl;
        return false;
    }
    
    themes_[theme.name] = theme;
    return true;
}

int ThemeManager::loadThemesFromDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        std::cerr << "ThemeManager: 主题目录不存在: " 
                  << dirPath.toStdString() << std::endl;
        return 0;
    }
    
    int count = 0;
    QStringList filters;
    filters << "*.yaml" << "*.yml";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& fileInfo : files) {
        if (loadThemeFromFile(fileInfo.absoluteFilePath())) {
            count++;
        }
    }
    
    return count;
}

Theme ThemeManager::parseThemeYaml(const QString& filePath) {
    Theme theme;
    
    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());
        
        // 读取主题名称
        if (root["name"]) {
            theme.name = QString::fromStdString(root["name"].as<std::string>());
        } else {
            // 使用文件名作为主题名称
            QFileInfo fileInfo(filePath);
            theme.name = fileInfo.baseName();
        }
        
        // 读取背景配置
        if (root["background"]) {
            auto bg = root["background"];
            if (bg["color"]) {
                theme.backgroundColor = QColor(QString::fromStdString(bg["color"].as<std::string>()));
            }
            if (bg["opacity"]) {
                theme.backgroundOpacity = bg["opacity"].as<int>();
            }
            if (bg["border_radius"]) {
                theme.borderRadius = bg["border_radius"].as<int>();
            }
            if (bg["border_color"]) {
                theme.borderColor = QColor(QString::fromStdString(bg["border_color"].as<std::string>()));
            }
            if (bg["border_width"]) {
                theme.borderWidth = bg["border_width"].as<int>();
            }
        }
        
        // 读取文字配置
        if (root["text"]) {
            auto text = root["text"];
            if (text["font_family"]) {
                theme.fontFamily = QString::fromStdString(text["font_family"].as<std::string>());
            }
            if (text["font_size"]) {
                theme.fontSize = text["font_size"].as<int>();
            }
            if (text["color"]) {
                theme.textColor = QColor(QString::fromStdString(text["color"].as<std::string>()));
            }
            if (text["highlight_text_color"]) {
                theme.highlightTextColor = QColor(QString::fromStdString(text["highlight_text_color"].as<std::string>()));
            }
            if (text["highlight_back_color"]) {
                theme.highlightBackColor = QColor(QString::fromStdString(text["highlight_back_color"].as<std::string>()));
            }
            if (text["preedit_color"]) {
                theme.preeditColor = QColor(QString::fromStdString(text["preedit_color"].as<std::string>()));
            }
            if (text["label_color"]) {
                theme.labelColor = QColor(QString::fromStdString(text["label_color"].as<std::string>()));
            }
            if (text["comment_color"]) {
                theme.commentColor = QColor(QString::fromStdString(text["comment_color"].as<std::string>()));
            }
        }
        
        // 读取间距配置
        if (root["spacing"]) {
            auto spacing = root["spacing"];
            if (spacing["candidate_spacing"]) {
                theme.candidateSpacing = spacing["candidate_spacing"].as<int>();
            }
            if (spacing["padding"]) {
                theme.padding = spacing["padding"].as<int>();
            }
        }
        
    } catch (const YAML::Exception& e) {
        std::cerr << "ThemeManager: YAML 解析错误: " << e.what() << std::endl;
        return Theme();  // 返回无效主题
    } catch (const std::exception& e) {
        std::cerr << "ThemeManager: 加载主题失败: " << e.what() << std::endl;
        return Theme();
    }
    
    return theme;
}

QStringList ThemeManager::getThemeNames() const {
    return themes_.keys();
}

Theme ThemeManager::getTheme(const QString& name) const {
    if (themes_.contains(name)) {
        return themes_[name];
    }
    // 返回默认浅色主题
    return Theme::defaultLight();
}

bool ThemeManager::hasTheme(const QString& name) const {
    return themes_.contains(name);
}

void ThemeManager::setCurrentTheme(const QString& name) {
    if (name == THEME_AUTO) {
        // 切换到跟随系统模式
        followSystem_ = true;
        currentThemeName_ = THEME_AUTO;
        applySystemTheme();
    } else if (themes_.contains(name)) {
        followSystem_ = false;
        currentThemeName_ = name;
        emit themeChanged(themes_[name]);
    } else {
        std::cerr << "ThemeManager: 主题不存在: " 
                  << name.toStdString() << std::endl;
    }
}

Theme ThemeManager::getCurrentTheme() const {
    if (followSystem_) {
        // 跟随系统时，根据系统深色模式返回对应主题
        return cachedSystemDarkMode_ ? themes_[THEME_DARK] : themes_[THEME_LIGHT];
    }
    
    if (themes_.contains(currentThemeName_)) {
        return themes_[currentThemeName_];
    }
    
    return Theme::defaultLight();
}

void ThemeManager::setFollowSystemTheme(bool follow) {
    if (followSystem_ != follow) {
        followSystem_ = follow;
        if (follow) {
            currentThemeName_ = THEME_AUTO;
            applySystemTheme();
        }
    }
}

bool ThemeManager::isSystemDarkMode() const {
    // 使用 Qt 6 的 QStyleHints 检测系统深色模式
    if (QGuiApplication::instance()) {
        auto colorScheme = QGuiApplication::styleHints()->colorScheme();
        return colorScheme == Qt::ColorScheme::Dark;
    }
    return false;
}

void ThemeManager::refreshSystemTheme() {
    bool isDark = isSystemDarkMode();
    if (cachedSystemDarkMode_ != isDark) {
        cachedSystemDarkMode_ = isDark;
        emit systemDarkModeChanged(isDark);
        
        if (followSystem_) {
            applySystemTheme();
        }
    }
}

void ThemeManager::setupSystemThemeMonitor() {
    // 使用 Qt 6 的 QStyleHints 监听系统主题变化
    if (QGuiApplication::instance()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme scheme) {
            bool isDark = (scheme == Qt::ColorScheme::Dark);
            if (cachedSystemDarkMode_ != isDark) {
                cachedSystemDarkMode_ = isDark;
                emit systemDarkModeChanged(isDark);
                
                if (followSystem_) {
                    applySystemTheme();
                }
            }
        });
    }
}

void ThemeManager::applySystemTheme() {
    Theme theme = cachedSystemDarkMode_ ? themes_[THEME_DARK] : themes_[THEME_LIGHT];
    emit themeChanged(theme);
}

void ThemeManager::syncFromConfigManager() {
    auto& configMgr = ConfigManager::instance();
    
    if (!configMgr.isInitialized()) {
        // ConfigManager 未初始化，使用默认配置（跟随系统）
        followSystem_ = true;
        currentThemeName_ = THEME_AUTO;
        applySystemTheme();
        return;
    }
    
    ThemeConfig themeConfig = configMgr.getThemeConfig();
    
    switch (themeConfig.mode) {
        case ThemeMode::Light:
            followSystem_ = false;
            currentThemeName_ = THEME_LIGHT;
            emit themeChanged(themes_[THEME_LIGHT]);
            break;
            
        case ThemeMode::Dark:
            followSystem_ = false;
            currentThemeName_ = THEME_DARK;
            emit themeChanged(themes_[THEME_DARK]);
            break;
            
        case ThemeMode::Auto:
        default:
            followSystem_ = true;
            currentThemeName_ = THEME_AUTO;
            applySystemTheme();
            break;
    }
    
    // 如果有自定义主题名称且存在该主题，使用自定义主题
    if (!themeConfig.customThemeName.empty() && themeConfig.mode != ThemeMode::Auto) {
        QString customName = QString::fromStdString(themeConfig.customThemeName);
        if (themes_.contains(customName)) {
            currentThemeName_ = customName;
            emit themeChanged(themes_[customName]);
        }
    }
}

void ThemeManager::setupConfigManagerConnection() {
    // 断开旧连接（如果有）
    if (configConnection_) {
        disconnect(configConnection_);
    }
    
    // 连接 ConfigManager 的主题配置变更信号
    configConnection_ = connect(&ConfigManager::instance(), &ConfigManager::themeConfigChanged,
                                this, [this](const ThemeConfig& config) {
        // 根据新配置更新主题
        switch (config.mode) {
            case ThemeMode::Light:
                followSystem_ = false;
                currentThemeName_ = THEME_LIGHT;
                emit themeChanged(themes_[THEME_LIGHT]);
                break;
                
            case ThemeMode::Dark:
                followSystem_ = false;
                currentThemeName_ = THEME_DARK;
                emit themeChanged(themes_[THEME_DARK]);
                break;
                
            case ThemeMode::Auto:
            default:
                followSystem_ = true;
                currentThemeName_ = THEME_AUTO;
                applySystemTheme();
                break;
        }
        
        // 如果有自定义主题名称且存在该主题，使用自定义主题
        if (!config.customThemeName.empty() && config.mode != ThemeMode::Auto) {
            QString customName = QString::fromStdString(config.customThemeName);
            if (themes_.contains(customName)) {
                currentThemeName_ = customName;
                emit themeChanged(themes_[customName]);
            }
        }
    });
}

} // namespace suyan
