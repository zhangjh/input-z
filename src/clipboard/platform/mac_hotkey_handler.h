/**
 * MacHotkeyHandler - macOS 全局快捷键处理器
 *
 * 使用 Carbon Event Manager 实现全局快捷键注册。
 * Carbon API 虽然是旧 API，但仍是 macOS 上注册全局快捷键的标准方式。
 *
 * 实现细节：
 * - 使用 RegisterEventHotKey 注册全局快捷键
 * - 使用 InstallApplicationEventHandler 安装事件处理器
 * - 快捷键签名使用 'SUYN'（SuYan 缩写）
 */

#ifndef SUYAN_CLIPBOARD_PLATFORM_MAC_HOTKEY_HANDLER_H
#define SUYAN_CLIPBOARD_PLATFORM_MAC_HOTKEY_HANDLER_H

#include "../hotkey_manager.h"
#include <map>

// Carbon 框架前向声明
typedef struct OpaqueEventHotKeyRef* EventHotKeyRef;
typedef struct OpaqueEventHandlerRef* EventHandlerRef;

namespace suyan {

/**
 * MacHotkeyHandler - macOS 快捷键处理器
 *
 * 实现 IHotkeyHandler 接口，使用 Carbon Event Manager。
 */
class MacHotkeyHandler : public IHotkeyHandler {
public:
    MacHotkeyHandler();
    ~MacHotkeyHandler() override;
    
    // ========== IHotkeyHandler 接口实现 ==========
    
    bool initialize(HotkeyManager* manager) override;
    void shutdown() override;
    bool registerHotkey(const Hotkey& hotkey, int hotkeyId) override;
    void unregisterHotkey(int hotkeyId) override;
    bool isHotkeyAvailable(const Hotkey& hotkey) const override;
    
    /**
     * 处理快捷键触发事件
     *
     * 由 Carbon 事件回调调用。
     *
     * @param hotkeyId 触发的快捷键 ID
     */
    void onHotkeyTriggered(int hotkeyId);
    
    // 快捷键签名（4 字符标识符）- 需要在回调函数中访问
    static constexpr unsigned int HOTKEY_SIGNATURE = 'SUYN';

private:
    /**
     * 安装 Carbon 事件处理器
     *
     * @return 是否成功
     */
    bool installEventHandler();
    
    /**
     * 移除 Carbon 事件处理器
     */
    void removeEventHandler();
    
    /**
     * 将 Hotkey 转换为 Carbon 修饰键
     *
     * @param hotkey 快捷键定义
     * @return Carbon 修饰键掩码
     */
    unsigned int hotkeyToModifiers(const Hotkey& hotkey) const;
    
    /**
     * 检查系统保留的快捷键
     *
     * @param hotkey 快捷键定义
     * @return 是否为系统保留
     */
    bool isSystemReserved(const Hotkey& hotkey) const;

    // 成员变量
    HotkeyManager* manager_ = nullptr;                      // 快捷键管理器
    EventHandlerRef eventHandler_ = nullptr;                // Carbon 事件处理器
    std::map<int, EventHotKeyRef> hotkeyRefs_;              // ID -> EventHotKeyRef 映射
    bool initialized_ = false;                               // 初始化状态
};

} // namespace suyan

#endif // SUYAN_CLIPBOARD_PLATFORM_MAC_HOTKEY_HANDLER_H
