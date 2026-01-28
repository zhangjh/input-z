/**
 * HotkeyManager - 全局快捷键管理器
 *
 * 提供跨平台的全局快捷键注册与管理功能。
 * 平台特定实现通过内部 handler 类隔离。
 *
 * 功能：
 * - 全局快捷键注册与注销
 * - 快捷键更新
 * - 快捷键冲突检测
 * - 快捷键触发信号
 */

#ifndef SUYAN_CLIPBOARD_HOTKEY_MANAGER_H
#define SUYAN_CLIPBOARD_HOTKEY_MANAGER_H

#include <QObject>
#include <QString>
#include <string>
#include <map>
#include <optional>
#include <memory>

namespace suyan {

/**
 * 快捷键定义
 *
 * 表示一个快捷键组合，包含主键码和修饰键。
 */
struct Hotkey {
    int keyCode = 0;            // 主键码（Qt::Key 或平台特定键码）
    bool ctrl = false;          // Ctrl/Cmd 修饰键（macOS 上映射到 Cmd）
    bool shift = false;         // Shift 修饰键
    bool alt = false;           // Alt/Option 修饰键
    bool meta = false;          // Meta/Win 修饰键（macOS 上为 Ctrl）
    
    /**
     * 转换为字符串表示
     *
     * 格式示例：
     * - "Cmd+Shift+V"
     * - "Ctrl+Alt+C"
     *
     * @return 快捷键字符串
     */
    std::string toString() const;
    
    /**
     * 从字符串解析快捷键
     *
     * 支持格式：
     * - "Cmd+Shift+V"
     * - "Ctrl+Alt+C"
     * - "Meta+V"
     *
     * @param str 快捷键字符串
     * @return 解析后的快捷键，解析失败返回空 Hotkey
     */
    static Hotkey fromString(const std::string& str);
    
    /**
     * 检查快捷键是否有效
     *
     * @return 是否有效（至少有一个修饰键和主键）
     */
    bool isValid() const;
    
    /**
     * 比较两个快捷键是否相同
     */
    bool operator==(const Hotkey& other) const;
    bool operator!=(const Hotkey& other) const;
};

// 前向声明平台特定 handler
class IHotkeyHandler;

/**
 * HotkeyManager - 快捷键管理类
 *
 * 单例模式，管理全局快捷键的注册、注销和触发。
 * 内部使用平台特定的 handler 实现。
 */
class HotkeyManager : public QObject {
    Q_OBJECT

public:
    /**
     * 获取单例实例
     *
     * @return HotkeyManager 单例引用
     */
    static HotkeyManager& instance();
    
    /**
     * 初始化快捷键管理器
     *
     * 必须在使用其他方法前调用。
     *
     * @return 是否成功初始化
     */
    bool initialize();
    
    /**
     * 关闭快捷键管理器
     *
     * 注销所有快捷键并释放资源。
     */
    void shutdown();
    
    /**
     * 检查是否已初始化
     *
     * @return 是否已初始化
     */
    bool isInitialized() const;
    
    /**
     * 注册快捷键
     *
     * @param name 快捷键名称（唯一标识）
     * @param hotkey 快捷键定义
     * @return 是否成功注册
     */
    bool registerHotkey(const std::string& name, const Hotkey& hotkey);
    
    /**
     * 注销快捷键
     *
     * @param name 快捷键名称
     */
    void unregisterHotkey(const std::string& name);
    
    /**
     * 注销所有快捷键
     */
    void unregisterAllHotkeys();
    
    /**
     * 更新快捷键
     *
     * 先注销旧快捷键，再注册新快捷键。
     *
     * @param name 快捷键名称
     * @param hotkey 新的快捷键定义
     * @return 是否成功更新
     */
    bool updateHotkey(const std::string& name, const Hotkey& hotkey);
    
    /**
     * 获取已注册的快捷键
     *
     * @param name 快捷键名称
     * @return 快捷键定义，未找到返回 std::nullopt
     */
    std::optional<Hotkey> getHotkey(const std::string& name) const;
    
    /**
     * 检查快捷键是否已注册
     *
     * @param name 快捷键名称
     * @return 是否已注册
     */
    bool hasHotkey(const std::string& name) const;
    
    /**
     * 检查快捷键是否可用（不与系统或其他应用冲突）
     *
     * 注意：此检测可能不完全准确，某些系统级快捷键无法检测。
     *
     * @param hotkey 要检查的快捷键
     * @return 是否可用
     */
    bool isHotkeyAvailable(const Hotkey& hotkey) const;
    
    /**
     * 获取所有已注册的快捷键
     *
     * @return 快捷键名称到定义的映射
     */
    const std::map<std::string, Hotkey>& getAllHotkeys() const;

signals:
    /**
     * 快捷键触发信号
     *
     * 当注册的快捷键被按下时发射。
     *
     * @param name 触发的快捷键名称
     */
    void hotkeyTriggered(const QString& name);
    
    /**
     * 快捷键注册成功信号
     *
     * @param name 快捷键名称
     */
    void hotkeyRegistered(const QString& name);
    
    /**
     * 快捷键注销信号
     *
     * @param name 快捷键名称
     */
    void hotkeyUnregistered(const QString& name);

private:
    // 私有构造函数（单例模式）
    HotkeyManager();
    ~HotkeyManager() override;
    
    // 禁止拷贝和移动
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    HotkeyManager(HotkeyManager&&) = delete;
    HotkeyManager& operator=(HotkeyManager&&) = delete;
    
    /**
     * 处理快捷键触发（由平台 handler 调用）
     *
     * @param hotkeyId 快捷键 ID
     */
    void onHotkeyTriggered(int hotkeyId);
    
    /**
     * 生成唯一的快捷键 ID
     *
     * @return 新的快捷键 ID
     */
    int generateHotkeyId();
    
    // 友元声明，允许平台 handler 访问私有方法
    friend class IHotkeyHandler;
    friend class MacHotkeyHandler;

    // 成员变量
    std::unique_ptr<IHotkeyHandler> handler_;       // 平台特定 handler
    std::map<std::string, Hotkey> hotkeys_;         // 名称 -> 快捷键映射
    std::map<std::string, int> hotkeyIds_;          // 名称 -> ID 映射
    std::map<int, std::string> idToName_;           // ID -> 名称映射
    int nextHotkeyId_ = 1;                          // 下一个可用 ID
    bool initialized_ = false;                       // 初始化状态
};

/**
 * IHotkeyHandler - 平台快捷键处理器接口
 *
 * 抽象基类，定义平台特定快捷键处理的接口。
 */
class IHotkeyHandler {
public:
    virtual ~IHotkeyHandler() = default;
    
    /**
     * 初始化处理器
     *
     * @param manager 快捷键管理器指针
     * @return 是否成功
     */
    virtual bool initialize(HotkeyManager* manager) = 0;
    
    /**
     * 关闭处理器
     */
    virtual void shutdown() = 0;
    
    /**
     * 注册快捷键
     *
     * @param hotkey 快捷键定义
     * @param hotkeyId 快捷键 ID
     * @return 是否成功
     */
    virtual bool registerHotkey(const Hotkey& hotkey, int hotkeyId) = 0;
    
    /**
     * 注销快捷键
     *
     * @param hotkeyId 快捷键 ID
     */
    virtual void unregisterHotkey(int hotkeyId) = 0;
    
    /**
     * 检查快捷键是否可用
     *
     * @param hotkey 快捷键定义
     * @return 是否可用
     */
    virtual bool isHotkeyAvailable(const Hotkey& hotkey) const = 0;
};

/**
 * 创建平台特定的快捷键处理器
 *
 * @return 处理器实例
 */
std::unique_ptr<IHotkeyHandler> createHotkeyHandler();

} // namespace suyan

#endif // SUYAN_CLIPBOARD_HOTKEY_MANAGER_H
