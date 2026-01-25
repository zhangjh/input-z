/**
 * IMKBridge - macOS Input Method Kit 桥接层实现
 *
 * 实现 SuYanInputController，处理 IMK 事件并转发到 InputEngine。
 * 
 * Task 8.1: 集成拼音输入流程
 * - 连接 IMK handleEvent 到 InputEngine::processKeyEvent
 * - 连接 InputEngine 状态变化到 CandidateWindow
 * - 连接 InputEngine 提交文本到 MacOSBridge
 *
 * Task 8.2: 集成中英文切换
 * - 实现 Shift 键切换中英文模式
 * - 实现大写字母开头临时英文模式
 * - 更新状态栏图标
 */

#include <QtCore/QPoint>
#include <QtWidgets/QWidget>

#ifdef Bool
#undef Bool
#endif

#import "IMKBridge.h"
#import <Carbon/Carbon.h>

#include "input_engine.h"
#include "macos_bridge.h"
#include "status_bar_manager.h"

#ifdef Bool
#undef Bool
#endif

#include "candidate_window.h"
#include "layout_manager.h"

using namespace suyan;

// ========== 全局变量 ==========

static InputEngine* g_inputEngine = nullptr;
static CandidateWindow* g_candidateWindow = nullptr;

// ========== C 桥接函数实现 ==========

extern "C" {

void SuYanIMK_SetInputEngine(void* engine) {
    g_inputEngine = static_cast<InputEngine*>(engine);
}

void* SuYanIMK_GetInputEngine(void) {
    return g_inputEngine;
}

void SuYanIMK_SetCandidateWindow(void* window) {
    g_candidateWindow = static_cast<CandidateWindow*>(window);
}

void* SuYanIMK_GetCandidateWindow(void) {
    return g_candidateWindow;
}

int SuYanIMK_ConvertKeyCode(unsigned short keyCode,
                            NSString* characters,
                            NSEventModifierFlags modifiers) {
    switch (keyCode) {
        case kVK_Return:        return 0xff0d;
        case kVK_Tab:           return 0xff09;
        case kVK_Space:         return 0x0020;
        case kVK_Delete:        return 0xff08;
        case kVK_Escape:        return 0xff1b;
        case kVK_ForwardDelete: return 0xffff;
        case kVK_Home:          return 0xff50;
        case kVK_End:           return 0xff57;
        case kVK_PageUp:        return 0xff55;
        case kVK_PageDown:      return 0xff56;
        case kVK_LeftArrow:     return 0xff51;
        case kVK_RightArrow:    return 0xff53;
        case kVK_UpArrow:       return 0xff52;
        case kVK_DownArrow:     return 0xff54;
        case kVK_Shift:         return 0xffe1;
        case kVK_RightShift:    return 0xffe2;
        case kVK_Control:       return 0xffe3;
        case kVK_RightControl:  return 0xffe4;
        case kVK_Option:        return 0xffe9;
        case kVK_RightOption:   return 0xffea;
        case kVK_Command:       return 0xffe7;
        case kVK_RightCommand:  return 0xffe8;
        case kVK_CapsLock:      return 0xffe5;
        case kVK_F1:            return 0xffbe;
        case kVK_F2:            return 0xffbf;
        case kVK_F3:            return 0xffc0;
        case kVK_F4:            return 0xffc1;
        case kVK_F5:            return 0xffc2;
        case kVK_F6:            return 0xffc3;
        case kVK_F7:            return 0xffc4;
        case kVK_F8:            return 0xffc5;
        case kVK_F9:            return 0xffc6;
        case kVK_F10:           return 0xffc7;
        case kVK_F11:           return 0xffc8;
        case kVK_F12:           return 0xffc9;
    }
    if (characters && characters.length > 0) {
        unichar ch = [characters characterAtIndex:0];
        if (ch >= 0x20 && ch <= 0x7e) {
            return ch;
        }
    }
    return 0;
}

int SuYanIMK_ConvertModifiers(NSEventModifierFlags modifierFlags) {
    int modifiers = 0;
    if (modifierFlags & NSEventModifierFlagShift) {
        modifiers |= KeyModifier::Shift;
    }
    if (modifierFlags & NSEventModifierFlagControl) {
        modifiers |= KeyModifier::Control;
    }
    if (modifierFlags & NSEventModifierFlagOption) {
        modifiers |= KeyModifier::Alt;
    }
    if (modifierFlags & NSEventModifierFlagCommand) {
        modifiers |= KeyModifier::Super;
    }
    return modifiers;
}

} // extern "C"

// ========== SuYanInputController 实现 ==========

@implementation SuYanInputController {
    id _currentClient;
    BOOL _isActive;
    NSString* _currentPreedit;
    
    // Shift 键状态跟踪（用于中英文切换）
    BOOL _shiftKeyPressed;           // Shift 键是否按下
    BOOL _otherKeyPressedWithShift;  // Shift 按下期间是否有其他键按下
    NSTimeInterval _shiftPressTime;  // Shift 按下的时间戳
    
    // 全局事件监听器（用于捕获 FlagsChanged 事件）
    id _flagsChangedMonitor;
}

- (instancetype)initWithServer:(IMKServer *)server
                      delegate:(id)delegate
                        client:(id)inputClient {
    self = [super initWithServer:server delegate:delegate client:inputClient];
    if (self) {
        _currentClient = inputClient;
        _isActive = NO;
        _currentPreedit = @"";
        _shiftKeyPressed = NO;
        _otherKeyPressedWithShift = NO;
        _shiftPressTime = 0;
        _flagsChangedMonitor = nil;
    }
    return self;
}

- (void)dealloc {
    [self stopFlagsChangedMonitor];
    [super dealloc];
}

/**
 * 启动 FlagsChanged 事件监听器
 *
 * 由于 IMK 不传递 FlagsChanged 事件，我们使用 NSEvent 的全局监听器来捕获
 * 注意：全局监听需要辅助功能权限
 */
- (void)startFlagsChangedMonitor {
    if (_flagsChangedMonitor) {
        return;  // 已经在监听
    }
    
    // 使用 __unsafe_unretained 代替 __weak（MRC 模式）
    __unsafe_unretained typeof(self) weakSelf = self;
    
    // 使用全局监听器来捕获所有应用的 FlagsChanged 事件
    _flagsChangedMonitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged
                                                                  handler:^(NSEvent * _Nonnull event) {
        [weakSelf handleFlagsChangedFromMonitor:event];
    }];
    
    if (_flagsChangedMonitor) {
        NSLog(@"SuYanInputController: Started global FlagsChanged monitor");
    } else {
        NSLog(@"SuYanInputController: Failed to start global FlagsChanged monitor (accessibility permission required?)");
    }
}

/**
 * 停止 FlagsChanged 事件监听器
 */
- (void)stopFlagsChangedMonitor {
    if (_flagsChangedMonitor) {
        [NSEvent removeMonitor:_flagsChangedMonitor];
        _flagsChangedMonitor = nil;
        NSLog(@"SuYanInputController: Stopped FlagsChanged monitor");
    }
}

/**
 * 处理来自监听器的 FlagsChanged 事件
 */
- (void)handleFlagsChangedFromMonitor:(NSEvent *)event {
    if (!_isActive) {
        return;  // 输入法未激活，忽略
    }
    
    NSEventModifierFlags flags = event.modifierFlags;
    BOOL shiftDown = (flags & NSEventModifierFlagShift) != 0;
    
    if (shiftDown && !_shiftKeyPressed) {
        // Shift 按下
        _shiftKeyPressed = YES;
        _otherKeyPressedWithShift = NO;
        _shiftPressTime = [NSDate timeIntervalSinceReferenceDate];
        NSLog(@"SuYanInputController: Shift pressed (from monitor)");
    }
    else if (!shiftDown && _shiftKeyPressed) {
        // Shift 释放
        NSTimeInterval elapsed = [NSDate timeIntervalSinceReferenceDate] - _shiftPressTime;
        NSLog(@"SuYanInputController: Shift released (from monitor), elapsed=%.3f, otherKeyPressed=%d",
              elapsed, _otherKeyPressedWithShift);
        
        if (!_otherKeyPressedWithShift && elapsed < 0.5 && g_inputEngine) {
            // 如果正在输入中文，提交当前的原始拼音字母（而不是清空）
            BOOL wasComposing = g_inputEngine->isComposing();
            NSLog(@"SuYanInputController: isComposing=%d, _currentClient=%p", wasComposing, _currentClient);
            
            if (wasComposing) {
                InputState state = g_inputEngine->getState();
                NSLog(@"SuYanInputController: rawInput='%s', preedit='%s'", 
                      state.rawInput.c_str(), state.preedit.c_str());
                
                if (!state.rawInput.empty()) {
                    NSString* textToCommit = [NSString stringWithUTF8String:state.rawInput.c_str()];
                    NSLog(@"SuYanInputController: About to commit: '%@'", textToCommit);
                    
                    // 先清除 preedit，再提交文本
                    [self clearPreedit];
                    [self commitText:textToCommit];
                    NSLog(@"SuYanInputController: Committed raw input: %s", state.rawInput.c_str());
                }
                g_inputEngine->reset();
                [self hideCandidateWindow];
            }
            
            g_inputEngine->toggleMode();
            [self updateStatusBarIcon];
            
            NSLog(@"SuYanInputController: Mode switched to %s (from monitor)",
                  g_inputEngine->getMode() == InputMode::Chinese ? "Chinese" : "English");
        }
        
        _shiftKeyPressed = NO;
        _otherKeyPressedWithShift = NO;
    }
}

/**
 * 处理键盘事件
 *
 * 这是输入流程的核心入口点：
 * 1. 更新 MacOSBridge 的 client（确保提交文本能正确发送）
 * 2. 处理 Shift 键切换中英文模式（FlagsChanged 事件）
 * 3. 将按键转换为 RIME 格式
 * 4. 调用 InputEngine::processKeyEvent 处理按键
 * 5. InputEngine 通过回调更新 CandidateWindow 和提交文本
 * 6. 更新候选词窗口位置和 preedit 显示
 *
 * 返回值说明：
 * - YES: 按键已被输入法处理，不传递给应用
 * - NO: 按键未被处理，传递给应用（如英文模式下的所有按键）
 */
- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
    _currentClient = sender;
    
    // 关键：更新 MacOSBridge 的 client
    [self updateMacOSBridgeClient];

    // 处理修饰键变化事件（Shift 键切换中英文）
    // 注意：IMK 通常不会传递 FlagsChanged 事件，我们使用全局监听器来处理
    if (event.type == NSEventTypeFlagsChanged) {
        NSLog(@"SuYanInputController: FlagsChanged event received in handleEvent, keyCode=%hu", event.keyCode);
        [self handleFlagsChanged:event];
        return NO;  // 修饰键事件不消费，让系统继续处理
    }

    // 只处理 KeyDown 事件
    if (event.type != NSEventTypeKeyDown) {
        return NO;
    }

    if (!g_inputEngine) {
        NSLog(@"SuYanInputController: InputEngine not available");
        return NO;
    }

    unsigned short keyCode = event.keyCode;
    NSString* characters = event.characters;
    NSEventModifierFlags modifierFlags = event.modifierFlags;
    
    // Command 键组合直接放行，让系统处理（如 Cmd+C/V/Z/A/X 等）
    if (modifierFlags & NSEventModifierFlagCommand) {
        NSLog(@"SuYanInputController: Command key combo detected, passing through (keyCode=%hu)", keyCode);
        return NO;
    }
    
    // Ctrl+Shift+L: 切换横排/竖排布局
    if ((modifierFlags & NSEventModifierFlagControl) && 
        (modifierFlags & NSEventModifierFlagShift) &&
        keyCode == kVK_ANSI_L) {
        [self toggleLayout:nil];
        return YES;
    }
    
    // Control 键组合也直接放行（如 Ctrl+A/E 等 Emacs 风格快捷键）
    if (modifierFlags & NSEventModifierFlagControl) {
        NSLog(@"SuYanInputController: Control key combo detected, passing through (keyCode=%hu)", keyCode);
        return NO;
    }
    
    // 如果 Shift 键按下期间有其他键按下，标记（用于判断是否切换模式）
    // 这个标记会被全局监听器使用
    if (_shiftKeyPressed && keyCode != kVK_Shift && keyCode != kVK_RightShift) {
        _otherKeyPressedWithShift = YES;
    }

    int rimeKeyCode = SuYanIMK_ConvertKeyCode(keyCode, characters, modifierFlags);
    int rimeModifiers = SuYanIMK_ConvertModifiers(modifierFlags);

    if (rimeKeyCode == 0) {
        return NO;
    }

    // 调用 InputEngine 处理按键
    BOOL handled = g_inputEngine->processKeyEvent(rimeKeyCode, rimeModifiers);

    // 如果按键被处理，更新候选词窗口和 preedit
    if (handled) {
        [self updateCandidateWindowPosition];
        [self updatePreeditDisplay];
    }

    return handled;
}

/**
 * 处理 Shift 键切换中英文模式
 *
 * 逻辑：
 * - Shift 按下时记录状态
 * - Shift 释放时，如果期间没有其他键按下，则切换模式
 * - 这样可以区分"单独按 Shift"和"Shift+其他键"
 *
 * @param event 键盘事件
 * @return YES 如果事件已处理（是 Shift 键事件）
 */
- (BOOL)handleShiftKeyForModeSwitch:(NSEvent *)event {
    // 此方法已废弃，使用 handleFlagsChanged 代替
    return NO;
}

/**
 * 处理修饰键变化事件
 *
 * 用于检测 Shift 键的按下和释放，实现中英文切换
 */
- (void)handleFlagsChanged:(NSEvent *)event {
    NSEventModifierFlags flags = event.modifierFlags;
    BOOL shiftDown = (flags & NSEventModifierFlagShift) != 0;
    
    NSLog(@"SuYanInputController: handleFlagsChanged shiftDown=%d, _shiftKeyPressed=%d, _otherKeyPressedWithShift=%d",
          shiftDown, _shiftKeyPressed, _otherKeyPressedWithShift);
    
    if (shiftDown && !_shiftKeyPressed) {
        // Shift 按下
        _shiftKeyPressed = YES;
        _otherKeyPressedWithShift = NO;
        _shiftPressTime = [NSDate timeIntervalSinceReferenceDate];
        NSLog(@"SuYanInputController: Shift pressed");
    }
    else if (!shiftDown && _shiftKeyPressed) {
        // Shift 释放
        NSTimeInterval elapsed = [NSDate timeIntervalSinceReferenceDate] - _shiftPressTime;
        NSLog(@"SuYanInputController: Shift released, elapsed=%.3f, otherKeyPressed=%d",
              elapsed, _otherKeyPressedWithShift);
        
        if (!_otherKeyPressedWithShift) {
            // 只有按下时间小于 0.5 秒才切换（避免长按 Shift）
            if (elapsed < 0.5 && g_inputEngine) {
                // 如果正在输入中文，先清空
                if (g_inputEngine->isComposing()) {
                    g_inputEngine->reset();
                    [self clearPreedit];
                    [self hideCandidateWindow];
                }
                
                g_inputEngine->toggleMode();
                [self updateStatusBarIcon];
                
                NSLog(@"SuYanInputController: Mode switched to %s",
                      g_inputEngine->getMode() == InputMode::Chinese ? "Chinese" : "English");
            } else {
                NSLog(@"SuYanInputController: Shift held too long (%.3f sec), not switching", elapsed);
            }
        } else {
            NSLog(@"SuYanInputController: Other key pressed with Shift, not switching");
        }
        
        _shiftKeyPressed = NO;
        _otherKeyPressedWithShift = NO;
    }
}

/**
 * 更新状态栏图标
 */
- (void)updateStatusBarIcon {
    if (!g_inputEngine) {
        return;
    }
    
    auto& statusBar = StatusBarManager::instance();
    statusBar.updateIcon(g_inputEngine->getMode());
}

/**
 * 更新 MacOSBridge 的 client
 *
 * 确保 InputEngine 提交文本时能正确发送到当前应用
 */
- (void)updateMacOSBridgeClient {
    if (g_inputEngine && g_inputEngine->getPlatformBridge()) {
        MacOSBridge* bridge = static_cast<MacOSBridge*>(g_inputEngine->getPlatformBridge());
        bridge->setClient((__bridge id)_currentClient);
    }
}

/**
 * 更新候选词窗口位置
 *
 * 根据当前输入状态决定显示或隐藏候选词窗口
 */
- (void)updateCandidateWindowPosition {
    if (!g_inputEngine || !g_candidateWindow) {
        return;
    }

    InputState state = g_inputEngine->getState();

    if (state.isComposing && !state.candidates.empty()) {
        // 获取光标位置并显示候选词窗口
        NSRect cursorRect = [self candidateWindowFrameForClient:_currentClient];
        NSScreen* screen = [NSScreen mainScreen];
        CGFloat screenHeight = screen.frame.size.height;
        
        // macOS 坐标系转换：原点从左下角转为左上角
        QPoint cursorPos(static_cast<int>(cursorRect.origin.x),
                        static_cast<int>(screenHeight - cursorRect.origin.y - cursorRect.size.height));
        g_candidateWindow->showAt(cursorPos);
    } else if (!state.isComposing) {
        [self hideCandidateWindow];
    }
}

/**
 * 更新 preedit 显示
 *
 * 在应用的输入框中显示当前输入的拼音
 */
- (void)updatePreeditDisplay {
    if (!g_inputEngine) {
        return;
    }

    InputState state = g_inputEngine->getState();

    if (!state.preedit.empty()) {
        NSString* preedit = [NSString stringWithUTF8String:state.preedit.c_str()];
        [self updatePreedit:preedit caretPosition:static_cast<NSInteger>(state.preedit.length())];
    } else {
        [self clearPreedit];
    }
}

- (void)activateServer:(id)sender {
    NSLog(@"SuYanInputController: activateServer called, sender=%@", sender);
    
    _currentClient = sender;
    _isActive = YES;
    
    // 重置 Shift 键状态
    _shiftKeyPressed = NO;
    _otherKeyPressedWithShift = NO;
    
    // 启动 FlagsChanged 事件监听器
    [self startFlagsChangedMonitor];

    // 更新 MacOSBridge 的 client
    [self updateMacOSBridgeClient];

    if (g_inputEngine) {
        g_inputEngine->activate();
        
        // 更新状态栏图标
        [self updateStatusBarIcon];
        NSLog(@"SuYanInputController: InputEngine activated, mode=%d", 
              static_cast<int>(g_inputEngine->getMode()));
    } else {
        NSLog(@"SuYanInputController: WARNING - InputEngine is NULL!");
    }

    NSLog(@"SuYanInputController: Server activated");
}

- (void)deactivateServer:(id)sender {
    [self commitComposition:sender];

    _isActive = NO;
    
    // 停止 FlagsChanged 事件监听器
    [self stopFlagsChangedMonitor];

    if (g_inputEngine) {
        g_inputEngine->deactivate();
    }

    [self hideCandidateWindow];

    NSLog(@"SuYanInputController: Server deactivated");
}

- (void)commitComposition:(id)sender {
    _currentClient = sender;
    
    // 更新 MacOSBridge 的 client
    [self updateMacOSBridgeClient];

    if (g_inputEngine && g_inputEngine->isComposing()) {
        g_inputEngine->commit();
    }

    [self clearPreedit];
    [self hideCandidateWindow];
}

- (NSMenu *)menu {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"素言"];

    NSMenuItem* modeItem = [[NSMenuItem alloc]
        initWithTitle:@"切换中/英文 (Shift)"
        action:@selector(toggleInputMode:)
        keyEquivalent:@""];
    modeItem.target = self;
    [menu addItem:modeItem];

    // 横排/竖排切换
    NSString* layoutTitle = @"切换横排/竖排 (⌃⇧L)";
    auto& layoutManager = LayoutManager::instance();
    if (layoutManager.isHorizontal()) {
        layoutTitle = @"切换为竖排 (⌃⇧L)";
    } else {
        layoutTitle = @"切换为横排 (⌃⇧L)";
    }
    NSMenuItem* layoutItem = [[NSMenuItem alloc]
        initWithTitle:layoutTitle
        action:@selector(toggleLayout:)
        keyEquivalent:@""];
    layoutItem.target = self;
    [menu addItem:layoutItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* settingsItem = [[NSMenuItem alloc]
        initWithTitle:@"设置..."
        action:@selector(openSettings:)
        keyEquivalent:@""];
    settingsItem.target = self;
    settingsItem.enabled = NO;
    [menu addItem:settingsItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* aboutItem = [[NSMenuItem alloc]
        initWithTitle:@"关于素言"
        action:@selector(showAbout:)
        keyEquivalent:@""];
    aboutItem.target = self;
    [menu addItem:aboutItem];

    return menu;
}

- (void)toggleInputMode:(id)sender {
    if (g_inputEngine) {
        // 如果正在输入，先清空
        if (g_inputEngine->isComposing()) {
            g_inputEngine->reset();
        }
        
        g_inputEngine->toggleMode();
        [self updateStatusBarIcon];
        [self updateCandidateWindowPosition];
        [self updatePreeditDisplay];
        
        NSLog(@"SuYanInputController: Mode toggled to %s",
              g_inputEngine->getMode() == InputMode::Chinese ? "Chinese" : "English");
    }
}

- (void)openSettings:(id)sender {
    NSLog(@"SuYanInputController: Open settings (not implemented)");
}

- (void)toggleLayout:(id)sender {
    auto& layoutManager = LayoutManager::instance();
    layoutManager.toggleLayout();
}

- (void)showAbout:(id)sender {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"素言输入法";
    alert.informativeText = @"版本 1.0.0\n\n基于 RIME 输入法引擎\n使用 Qt 构建跨平台 UI";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"确定"];
    [alert runModal];
}

- (NSRect)candidateWindowFrameForClient:(id)sender {
    NSRect cursorRect = NSZeroRect;

    @try {
        [sender attributesForCharacterIndex:0 lineHeightRectangle:&cursorRect];
    } @catch (NSException* exception) {
        NSLog(@"SuYanInputController: Failed to get cursor position: %@", exception);
        cursorRect = NSMakeRect(100, 100, 0, 20);
    }

    return cursorRect;
}

/**
 * 更新候选词窗口（兼容旧接口）
 *
 * 注意：候选词内容现在通过 InputEngine 的回调更新
 * 这个方法主要用于更新窗口位置和 preedit
 */
- (void)updateCandidateWindow {
    [self updateCandidateWindowPosition];
    [self updatePreeditDisplay];
}

- (void)hideCandidateWindow {
    if (g_candidateWindow) {
        g_candidateWindow->hideWindow();
    }
}

- (void)commitText:(NSString *)text {
    if (!_currentClient || !text || text.length == 0) {
        return;
    }

    @try {
        [_currentClient insertText:text
                  replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
    } @catch (NSException* exception) {
        NSLog(@"SuYanInputController: Failed to commit text: %@", exception);
    }
}

- (void)updatePreedit:(NSString *)preedit caretPosition:(NSInteger)caretPos {
    if (!_currentClient) {
        return;
    }

    _currentPreedit = preedit;

    @try {
        NSDictionary* attrs = @{
            NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle),
            NSForegroundColorAttributeName: [NSColor textColor]
        };
        NSAttributedString* attrString = [[NSAttributedString alloc]
            initWithString:preedit
            attributes:attrs];

        [_currentClient setMarkedText:attrString
                       selectionRange:NSMakeRange(static_cast<NSUInteger>(caretPos), 0)
                     replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
    } @catch (NSException* exception) {
        NSLog(@"SuYanInputController: Failed to update preedit: %@", exception);
    }
}

- (void)clearPreedit {
    if (!_currentClient) {
        return;
    }

    _currentPreedit = @"";

    @try {
        [_currentClient setMarkedText:@""
                       selectionRange:NSMakeRange(0, 0)
                     replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
    } @catch (NSException* exception) {
        NSLog(@"SuYanInputController: Failed to clear preedit: %@", exception);
    }
}

@end
