/**
 * MacHotkeyHandler - macOS 全局快捷键处理器实现
 *
 * 使用 Carbon Event Manager 实现全局快捷键。
 */

#import "mac_hotkey_handler.h"
#import <Carbon/Carbon.h>
#import <QDebug>
#import <QTimer>

namespace suyan {

// ============================================================================
// Carbon 事件回调函数
// ============================================================================

/**
 * Carbon 快捷键事件回调
 *
 * 当注册的全局快捷键被按下时，系统调用此函数。
 * 使用 QTimer::singleShot 确保在 Qt 主线程中处理。
 */
static OSStatus hotkeyEventCallback(EventHandlerCallRef nextHandler,
                                     EventRef event,
                                     void* userData) {
    Q_UNUSED(nextHandler);
    
    MacHotkeyHandler* handler = static_cast<MacHotkeyHandler*>(userData);
    if (!handler) {
        return eventNotHandledErr;
    }
    
    // 获取快捷键 ID
    EventHotKeyID hotkeyID;
    OSStatus status = GetEventParameter(event,
                                         kEventParamDirectObject,
                                         typeEventHotKeyID,
                                         nullptr,
                                         sizeof(hotkeyID),
                                         nullptr,
                                         &hotkeyID);
    
    if (status != noErr) {
        qWarning() << "MacHotkeyHandler: Failed to get hotkey ID from event";
        return eventNotHandledErr;
    }
    
    // 验证签名
    if (hotkeyID.signature != MacHotkeyHandler::HOTKEY_SIGNATURE) {
        return eventNotHandledErr;
    }
    
    // 使用 QTimer::singleShot 在 Qt 主线程中触发回调
    // 这样可以避免 Carbon 事件线程和 Qt 事件循环的冲突
    int hotkeyIdCopy = hotkeyID.id;
    QTimer::singleShot(0, [handler, hotkeyIdCopy]() {
        handler->onHotkeyTriggered(hotkeyIdCopy);
    });
    
    return noErr;
}

// ============================================================================
// MacHotkeyHandler 实现
// ============================================================================

MacHotkeyHandler::MacHotkeyHandler() = default;

MacHotkeyHandler::~MacHotkeyHandler() {
    shutdown();
}

bool MacHotkeyHandler::initialize(HotkeyManager* manager) {
    if (initialized_) {
        qDebug() << "MacHotkeyHandler: Already initialized";
        return true;
    }
    
    if (!manager) {
        qWarning() << "MacHotkeyHandler: Null manager pointer";
        return false;
    }
    
    manager_ = manager;
    
    // 安装事件处理器
    if (!installEventHandler()) {
        qWarning() << "MacHotkeyHandler: Failed to install event handler";
        return false;
    }
    
    initialized_ = true;
    qDebug() << "MacHotkeyHandler: Initialized successfully";
    return true;
}

void MacHotkeyHandler::shutdown() {
    if (!initialized_) {
        return;
    }
    
    // 注销所有快捷键
    for (auto& pair : hotkeyRefs_) {
        if (pair.second) {
            UnregisterEventHotKey(pair.second);
        }
    }
    hotkeyRefs_.clear();
    
    // 移除事件处理器
    removeEventHandler();
    
    manager_ = nullptr;
    initialized_ = false;
    
    qDebug() << "MacHotkeyHandler: Shutdown complete";
}

bool MacHotkeyHandler::registerHotkey(const Hotkey& hotkey, int hotkeyId) {
    if (!initialized_) {
        qWarning() << "MacHotkeyHandler: Not initialized";
        return false;
    }
    
    // 检查是否已注册
    if (hotkeyRefs_.find(hotkeyId) != hotkeyRefs_.end()) {
        qWarning() << "MacHotkeyHandler: Hotkey ID already registered:" << hotkeyId;
        return false;
    }
    
    // 构建 EventHotKeyID
    EventHotKeyID eventHotkeyID;
    eventHotkeyID.signature = HOTKEY_SIGNATURE;
    eventHotkeyID.id = hotkeyId;
    
    // 转换修饰键
    UInt32 modifiers = hotkeyToModifiers(hotkey);
    
    // 注册快捷键
    EventHotKeyRef hotkeyRef = nullptr;
    OSStatus status = RegisterEventHotKey(
        hotkey.keyCode,
        modifiers,
        eventHotkeyID,
        GetApplicationEventTarget(),
        0,  // options
        &hotkeyRef
    );
    
    if (status != noErr) {
        qWarning() << "MacHotkeyHandler: RegisterEventHotKey failed with status:" << status;
        return false;
    }
    
    // 保存引用
    hotkeyRefs_[hotkeyId] = hotkeyRef;
    
    qDebug() << "MacHotkeyHandler: Registered hotkey ID:" << hotkeyId
             << "keyCode:" << hotkey.keyCode
             << "modifiers:" << modifiers;
    
    return true;
}

void MacHotkeyHandler::unregisterHotkey(int hotkeyId) {
    auto it = hotkeyRefs_.find(hotkeyId);
    if (it == hotkeyRefs_.end()) {
        return;
    }
    
    if (it->second) {
        OSStatus status = UnregisterEventHotKey(it->second);
        if (status != noErr) {
            qWarning() << "MacHotkeyHandler: UnregisterEventHotKey failed with status:" << status;
        }
    }
    
    hotkeyRefs_.erase(it);
    
    qDebug() << "MacHotkeyHandler: Unregistered hotkey ID:" << hotkeyId;
}

bool MacHotkeyHandler::isHotkeyAvailable(const Hotkey& hotkey) const {
    // 检查系统保留的快捷键
    if (isSystemReserved(hotkey)) {
        return false;
    }
    
    // 尝试临时注册来检测可用性
    // 注意：这种方法不完美，某些系统快捷键可能无法检测
    EventHotKeyID testID;
    testID.signature = 'TEST';
    testID.id = 0;
    
    UInt32 modifiers = hotkeyToModifiers(hotkey);
    EventHotKeyRef testRef = nullptr;
    
    OSStatus status = RegisterEventHotKey(
        hotkey.keyCode,
        modifiers,
        testID,
        GetApplicationEventTarget(),
        0,
        &testRef
    );
    
    if (status == noErr && testRef) {
        // 注册成功，说明可用
        UnregisterEventHotKey(testRef);
        return true;
    }
    
    return false;
}

void MacHotkeyHandler::onHotkeyTriggered(int hotkeyId) {
    if (manager_) {
        // 调用管理器的私有方法
        manager_->onHotkeyTriggered(hotkeyId);
    }
}

bool MacHotkeyHandler::installEventHandler() {
    // 定义要处理的事件类型
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyPressed;
    
    // 安装事件处理器
    OSStatus status = InstallApplicationEventHandler(
        &hotkeyEventCallback,
        1,
        &eventType,
        this,
        &eventHandler_
    );
    
    if (status != noErr) {
        qWarning() << "MacHotkeyHandler: InstallApplicationEventHandler failed with status:" << status;
        return false;
    }
    
    qDebug() << "MacHotkeyHandler: Event handler installed";
    return true;
}

void MacHotkeyHandler::removeEventHandler() {
    if (eventHandler_) {
        RemoveEventHandler(eventHandler_);
        eventHandler_ = nullptr;
        qDebug() << "MacHotkeyHandler: Event handler removed";
    }
}

unsigned int MacHotkeyHandler::hotkeyToModifiers(const Hotkey& hotkey) const {
    UInt32 modifiers = 0;
    
    // macOS 修饰键映射：
    // - ctrl 字段映射到 Cmd 键（cmdKey）
    // - shift 字段映射到 Shift 键（shiftKey）
    // - alt 字段映射到 Option 键（optionKey）
    // - meta 字段映射到 Control 键（controlKey）
    
    if (hotkey.ctrl) {
        modifiers |= cmdKey;
    }
    if (hotkey.shift) {
        modifiers |= shiftKey;
    }
    if (hotkey.alt) {
        modifiers |= optionKey;
    }
    if (hotkey.meta) {
        modifiers |= controlKey;
    }
    
    return modifiers;
}

bool MacHotkeyHandler::isSystemReserved(const Hotkey& hotkey) const {
    // 检查一些常见的系统保留快捷键
    // 注意：这个列表不完整，仅作为基本检查
    
    // Cmd+Tab (应用切换)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x30) {
        return true;
    }
    
    // Cmd+Space (Spotlight)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x31) {
        return true;
    }
    
    // Cmd+Q (退出应用)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x0C) {
        return true;
    }
    
    // Cmd+H (隐藏应用)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x04) {
        return true;
    }
    
    // Cmd+M (最小化窗口)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x2E) {
        return true;
    }
    
    // Cmd+W (关闭窗口)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && !hotkey.meta && hotkey.keyCode == 0x0D) {
        return true;
    }
    
    // Ctrl+Cmd+Q (锁屏)
    if (hotkey.ctrl && !hotkey.shift && !hotkey.alt && hotkey.meta && hotkey.keyCode == 0x0C) {
        return true;
    }
    
    return false;
}

// ============================================================================
// 工厂函数实现
// ============================================================================

std::unique_ptr<IHotkeyHandler> createHotkeyHandler() {
    return std::make_unique<MacHotkeyHandler>();
}

} // namespace suyan
