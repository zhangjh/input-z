/**
 * StatusBarManager - macOS 状态栏图标管理器实现
 */

// 先包含 Qt 相关头文件（避免与 RIME 的 Bool 宏冲突）
#include "config_manager.h"
#include "clipboard_manager.h"
#include "hotkey_manager.h"

#import <Cocoa/Cocoa.h>

// 在包含 RIME 相关头文件之前取消 Bool 宏定义
#ifdef Bool
#undef Bool
#endif

#include "status_bar_manager.h"
#include "input_engine.h"

// ========== 剪贴板菜单 Action Handler ==========

/**
 * ClipboardMenuHandler - 剪贴板菜单事件处理器
 *
 * 处理剪贴板设置菜单的各种操作。
 */
@interface ClipboardMenuHandler : NSObject

+ (instancetype)sharedHandler;

// 菜单操作
- (void)toggleClipboardEnabled:(id)sender;
- (void)setRetentionDays:(id)sender;
- (void)setCustomRetentionDays:(id)sender;
- (void)setMaxCount:(id)sender;
- (void)setCustomMaxCount:(id)sender;
- (void)changeHotkey:(id)sender;
- (void)clearHistory:(id)sender;

@end

@implementation ClipboardMenuHandler

+ (instancetype)sharedHandler {
    static ClipboardMenuHandler* instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[ClipboardMenuHandler alloc] init];
    });
    return instance;
}

- (void)toggleClipboardEnabled:(id)sender {
    auto& config = suyan::ConfigManager::instance();
    auto& clipboard = suyan::ClipboardManager::instance();
    
    bool currentEnabled = config.getClipboardConfig().enabled;
    bool newEnabled = !currentEnabled;
    
    config.setClipboardEnabled(newEnabled);
    clipboard.setEnabled(newEnabled);
    
    if (newEnabled) {
        clipboard.startMonitoring();
    } else {
        clipboard.stopMonitoring();
    }
    
    NSLog(@"ClipboardMenuHandler: Clipboard %@", newEnabled ? @"enabled" : @"disabled");
}

- (void)setRetentionDays:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    int days = (int)item.tag;
    
    auto& config = suyan::ConfigManager::instance();
    auto& clipboard = suyan::ClipboardManager::instance();
    
    config.setClipboardMaxAgeDays(days);
    clipboard.setMaxAgeDays(days);
    
    NSLog(@"ClipboardMenuHandler: Retention days set to %d", days);
}

- (void)setCustomRetentionDays:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"自定义保留天数";
    alert.informativeText = @"请输入保留天数（1-365）：";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"确定"];
    [alert addButtonWithTitle:@"取消"];
    
    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    auto& config = suyan::ConfigManager::instance();
    input.stringValue = [NSString stringWithFormat:@"%d", config.getClipboardConfig().maxAgeDays];
    alert.accessoryView = input;
    
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        int days = input.intValue;
        if (days >= 1 && days <= 365) {
            auto& clipboard = suyan::ClipboardManager::instance();
            config.setClipboardMaxAgeDays(days);
            clipboard.setMaxAgeDays(days);
            NSLog(@"ClipboardMenuHandler: Custom retention days set to %d", days);
        } else {
            NSAlert* errorAlert = [[NSAlert alloc] init];
            errorAlert.messageText = @"输入无效";
            errorAlert.informativeText = @"请输入 1-365 之间的数字。";
            errorAlert.alertStyle = NSAlertStyleWarning;
            [errorAlert addButtonWithTitle:@"确定"];
            [errorAlert runModal];
        }
    }
}

- (void)setMaxCount:(id)sender {
    NSMenuItem* item = (NSMenuItem*)sender;
    int count = (int)item.tag;
    
    auto& config = suyan::ConfigManager::instance();
    auto& clipboard = suyan::ClipboardManager::instance();
    
    config.setClipboardMaxCount(count);
    clipboard.setMaxCount(count);
    
    NSLog(@"ClipboardMenuHandler: Max count set to %d", count);
}

- (void)setCustomMaxCount:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"自定义最大条数";
    alert.informativeText = @"请输入最大保留条数（100-10000）：";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"确定"];
    [alert addButtonWithTitle:@"取消"];
    
    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    auto& config = suyan::ConfigManager::instance();
    input.stringValue = [NSString stringWithFormat:@"%d", config.getClipboardConfig().maxCount];
    alert.accessoryView = input;
    
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        int count = input.intValue;
        if (count >= 100 && count <= 10000) {
            auto& clipboard = suyan::ClipboardManager::instance();
            config.setClipboardMaxCount(count);
            clipboard.setMaxCount(count);
            NSLog(@"ClipboardMenuHandler: Custom max count set to %d", count);
        } else {
            NSAlert* errorAlert = [[NSAlert alloc] init];
            errorAlert.messageText = @"输入无效";
            errorAlert.informativeText = @"请输入 100-10000 之间的数字。";
            errorAlert.alertStyle = NSAlertStyleWarning;
            [errorAlert addButtonWithTitle:@"确定"];
            [errorAlert runModal];
        }
    }
}

- (void)changeHotkey:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"修改快捷键";
    alert.informativeText = @"请输入新的快捷键（如 Cmd+Shift+V）：";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"确定"];
    [alert addButtonWithTitle:@"取消"];
    
    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    auto& config = suyan::ConfigManager::instance();
    input.stringValue = [NSString stringWithUTF8String:config.getClipboardConfig().hotkey.c_str()];
    alert.accessoryView = input;
    
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        std::string hotkeyStr = [input.stringValue UTF8String];
        suyan::Hotkey newHotkey = suyan::Hotkey::fromString(hotkeyStr);
        
        if (newHotkey.isValid()) {
            auto& hotkeyManager = suyan::HotkeyManager::instance();
            
            // 检查快捷键是否可用
            if (hotkeyManager.isHotkeyAvailable(newHotkey)) {
                // 更新快捷键
                if (hotkeyManager.updateHotkey("clipboard_toggle", newHotkey)) {
                    config.setClipboardHotkey(hotkeyStr);
                    NSLog(@"ClipboardMenuHandler: Hotkey changed to %s", hotkeyStr.c_str());
                } else {
                    NSAlert* errorAlert = [[NSAlert alloc] init];
                    errorAlert.messageText = @"快捷键注册失败";
                    errorAlert.informativeText = @"无法注册该快捷键，请尝试其他组合。";
                    errorAlert.alertStyle = NSAlertStyleWarning;
                    [errorAlert addButtonWithTitle:@"确定"];
                    [errorAlert runModal];
                }
            } else {
                NSAlert* errorAlert = [[NSAlert alloc] init];
                errorAlert.messageText = @"快捷键冲突";
                errorAlert.informativeText = @"该快捷键可能与系统或其他应用冲突，请尝试其他组合。";
                errorAlert.alertStyle = NSAlertStyleWarning;
                [errorAlert addButtonWithTitle:@"确定"];
                [errorAlert runModal];
            }
        } else {
            NSAlert* errorAlert = [[NSAlert alloc] init];
            errorAlert.messageText = @"快捷键格式无效";
            errorAlert.informativeText = @"请使用正确的格式，如 Cmd+Shift+V。";
            errorAlert.alertStyle = NSAlertStyleWarning;
            [errorAlert addButtonWithTitle:@"确定"];
            [errorAlert runModal];
        }
    }
}

- (void)clearHistory:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"清空剪贴板历史";
    alert.informativeText = @"确定要清空所有剪贴板历史记录吗？此操作不可撤销。";
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:@"清空"];
    [alert addButtonWithTitle:@"取消"];
    
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        auto& clipboard = suyan::ClipboardManager::instance();
        clipboard.clearHistory();
        NSLog(@"ClipboardMenuHandler: History cleared");
    }
}

@end

namespace suyan {

// ========== 单例实现 ==========

StatusBarManager& StatusBarManager::instance() {
    static StatusBarManager instance;
    return instance;
}

StatusBarManager::StatusBarManager() = default;

StatusBarManager::~StatusBarManager() = default;

// ========== 公共方法 ==========

bool StatusBarManager::initialize(const std::string& resourcePath) {
    if (initialized_) {
        return true;
    }

    resourcePath_ = resourcePath;
    initialized_ = true;
    
    NSLog(@"StatusBarManager: Initialized with resource path: %s", resourcePath.c_str());
    return true;
}

void StatusBarManager::updateIcon(InputMode mode) {
    switch (mode) {
        case InputMode::Chinese:
            setIconType(StatusIconType::Chinese);
            break;
        case InputMode::English:
        case InputMode::TempEnglish:
            setIconType(StatusIconType::English);
            break;
    }
}

void StatusBarManager::setIconType(StatusIconType type) {
    if (currentIconType_ == type) {
        return;
    }

    currentIconType_ = type;
    
    // macOS 输入法的状态栏图标由系统管理
    // 我们无法直接更改系统菜单栏中的输入法图标
    // 但可以通过以下方式提供视觉反馈：
    // 1. 在候选词窗口中显示当前模式
    // 2. 在输入法菜单中显示当前模式
    // 3. 使用 Dock 图标（但输入法通常隐藏 Dock 图标）
    
    NSLog(@"StatusBarManager: Icon type changed to %s", 
          type == StatusIconType::Chinese ? "Chinese" : "English");
}

StatusIconType StatusBarManager::getCurrentIconType() const {
    return currentIconType_;
}

std::string StatusBarManager::getModeText() const {
    switch (currentIconType_) {
        case StatusIconType::Chinese:
            return "中";
        case StatusIconType::English:
            return "A";
        case StatusIconType::Disabled:
            return "-";
    }
    return "?";
}

// ========== 剪贴板菜单实现 ==========

NSMenu* StatusBarManager::createClipboardMenu() {
    NSMenu* clipboardMenu = [[NSMenu alloc] initWithTitle:@"剪贴板"];
    ClipboardMenuHandler* handler = [ClipboardMenuHandler sharedHandler];
    
    // 获取当前配置
    auto& config = ConfigManager::instance();
    ClipboardConfig clipboardConfig = config.getClipboardConfig();
    
    // 1. 启用/禁用剪贴板（复选框）
    NSMenuItem* enableItem = [[NSMenuItem alloc]
        initWithTitle:@"启用剪贴板"
        action:@selector(toggleClipboardEnabled:)
        keyEquivalent:@""];
    enableItem.target = handler;
    enableItem.state = clipboardConfig.enabled ? NSControlStateValueOn : NSControlStateValueOff;
    [clipboardMenu addItem:enableItem];
    
    [clipboardMenu addItem:[NSMenuItem separatorItem]];
    
    // 2. 保留时长子菜单
    NSMenuItem* retentionItem = [[NSMenuItem alloc]
        initWithTitle:@"保留时长"
        action:nil
        keyEquivalent:@""];
    NSMenu* retentionMenu = [[NSMenu alloc] initWithTitle:@"保留时长"];
    
    // 1周
    NSMenuItem* week1Item = [[NSMenuItem alloc]
        initWithTitle:@"1 周"
        action:@selector(setRetentionDays:)
        keyEquivalent:@""];
    week1Item.target = handler;
    week1Item.tag = 7;
    week1Item.state = (clipboardConfig.maxAgeDays == 7) ? NSControlStateValueOn : NSControlStateValueOff;
    [retentionMenu addItem:week1Item];
    
    // 1月
    NSMenuItem* month1Item = [[NSMenuItem alloc]
        initWithTitle:@"1 月"
        action:@selector(setRetentionDays:)
        keyEquivalent:@""];
    month1Item.target = handler;
    month1Item.tag = 30;
    month1Item.state = (clipboardConfig.maxAgeDays == 30) ? NSControlStateValueOn : NSControlStateValueOff;
    [retentionMenu addItem:month1Item];
    
    // 3月
    NSMenuItem* month3Item = [[NSMenuItem alloc]
        initWithTitle:@"3 月"
        action:@selector(setRetentionDays:)
        keyEquivalent:@""];
    month3Item.target = handler;
    month3Item.tag = 90;
    month3Item.state = (clipboardConfig.maxAgeDays == 90) ? NSControlStateValueOn : NSControlStateValueOff;
    [retentionMenu addItem:month3Item];
    
    [retentionMenu addItem:[NSMenuItem separatorItem]];
    
    // 自定义
    NSMenuItem* customRetentionItem = [[NSMenuItem alloc]
        initWithTitle:[NSString stringWithFormat:@"自定义... (当前: %d 天)", clipboardConfig.maxAgeDays]
        action:@selector(setCustomRetentionDays:)
        keyEquivalent:@""];
    customRetentionItem.target = handler;
    [retentionMenu addItem:customRetentionItem];
    
    retentionItem.submenu = retentionMenu;
    [clipboardMenu addItem:retentionItem];
    
    // 3. 最大条数子菜单
    NSMenuItem* maxCountItem = [[NSMenuItem alloc]
        initWithTitle:@"最大条数"
        action:nil
        keyEquivalent:@""];
    NSMenu* maxCountMenu = [[NSMenu alloc] initWithTitle:@"最大条数"];
    
    // 500
    NSMenuItem* count500Item = [[NSMenuItem alloc]
        initWithTitle:@"500 条"
        action:@selector(setMaxCount:)
        keyEquivalent:@""];
    count500Item.target = handler;
    count500Item.tag = 500;
    count500Item.state = (clipboardConfig.maxCount == 500) ? NSControlStateValueOn : NSControlStateValueOff;
    [maxCountMenu addItem:count500Item];
    
    // 1000
    NSMenuItem* count1000Item = [[NSMenuItem alloc]
        initWithTitle:@"1000 条"
        action:@selector(setMaxCount:)
        keyEquivalent:@""];
    count1000Item.target = handler;
    count1000Item.tag = 1000;
    count1000Item.state = (clipboardConfig.maxCount == 1000) ? NSControlStateValueOn : NSControlStateValueOff;
    [maxCountMenu addItem:count1000Item];
    
    // 2000
    NSMenuItem* count2000Item = [[NSMenuItem alloc]
        initWithTitle:@"2000 条"
        action:@selector(setMaxCount:)
        keyEquivalent:@""];
    count2000Item.target = handler;
    count2000Item.tag = 2000;
    count2000Item.state = (clipboardConfig.maxCount == 2000) ? NSControlStateValueOn : NSControlStateValueOff;
    [maxCountMenu addItem:count2000Item];
    
    [maxCountMenu addItem:[NSMenuItem separatorItem]];
    
    // 自定义
    NSMenuItem* customCountItem = [[NSMenuItem alloc]
        initWithTitle:[NSString stringWithFormat:@"自定义... (当前: %d 条)", clipboardConfig.maxCount]
        action:@selector(setCustomMaxCount:)
        keyEquivalent:@""];
    customCountItem.target = handler;
    [maxCountMenu addItem:customCountItem];
    
    maxCountItem.submenu = maxCountMenu;
    [clipboardMenu addItem:maxCountItem];
    
    [clipboardMenu addItem:[NSMenuItem separatorItem]];
    
    // 4. 修改快捷键
    NSMenuItem* hotkeyItem = [[NSMenuItem alloc]
        initWithTitle:[NSString stringWithFormat:@"快捷键: %s", clipboardConfig.hotkey.c_str()]
        action:@selector(changeHotkey:)
        keyEquivalent:@""];
    hotkeyItem.target = handler;
    [clipboardMenu addItem:hotkeyItem];
    
    [clipboardMenu addItem:[NSMenuItem separatorItem]];
    
    // 5. 清空历史
    NSMenuItem* clearItem = [[NSMenuItem alloc]
        initWithTitle:@"清空历史..."
        action:@selector(clearHistory:)
        keyEquivalent:@""];
    clearItem.target = handler;
    [clipboardMenu addItem:clearItem];
    
    return clipboardMenu;
}

void StatusBarManager::updateClipboardMenuState(NSMenu* menu) {
    if (!menu) {
        return;
    }
    
    auto& config = ConfigManager::instance();
    ClipboardConfig clipboardConfig = config.getClipboardConfig();
    
    // 更新启用状态
    NSMenuItem* enableItem = [menu itemAtIndex:0];
    if (enableItem) {
        enableItem.state = clipboardConfig.enabled ? NSControlStateValueOn : NSControlStateValueOff;
    }
    
    // 更新保留时长子菜单
    NSMenuItem* retentionItem = [menu itemWithTitle:@"保留时长"];
    if (retentionItem && retentionItem.submenu) {
        NSMenu* retentionMenu = retentionItem.submenu;
        for (NSMenuItem* item in retentionMenu.itemArray) {
            if (item.action == @selector(setRetentionDays:)) {
                item.state = (item.tag == clipboardConfig.maxAgeDays) ? NSControlStateValueOn : NSControlStateValueOff;
            } else if (item.action == @selector(setCustomRetentionDays:)) {
                item.title = [NSString stringWithFormat:@"自定义... (当前: %d 天)", clipboardConfig.maxAgeDays];
            }
        }
    }
    
    // 更新最大条数子菜单
    NSMenuItem* maxCountItem = [menu itemWithTitle:@"最大条数"];
    if (maxCountItem && maxCountItem.submenu) {
        NSMenu* maxCountMenu = maxCountItem.submenu;
        for (NSMenuItem* item in maxCountMenu.itemArray) {
            if (item.action == @selector(setMaxCount:)) {
                item.state = (item.tag == clipboardConfig.maxCount) ? NSControlStateValueOn : NSControlStateValueOff;
            } else if (item.action == @selector(setCustomMaxCount:)) {
                item.title = [NSString stringWithFormat:@"自定义... (当前: %d 条)", clipboardConfig.maxCount];
            }
        }
    }
    
    // 更新快捷键显示
    for (NSMenuItem* item in menu.itemArray) {
        if (item.action == @selector(changeHotkey:)) {
            item.title = [NSString stringWithFormat:@"快捷键: %s", clipboardConfig.hotkey.c_str()];
            break;
        }
    }
}

} // namespace suyan
