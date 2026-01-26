# Requirements Document

## Introduction

本文档定义了素言输入法（SuYan）Windows 平台支持的需求。Windows 实现需要通过 TSF (Text Services Framework) 与系统集成，同时复用现有的跨平台核心层（InputEngine、RimeWrapper）和 UI 层（CandidateWindow、ThemeManager）。

目标是实现与 macOS 版本功能一致的输入体验，包括拼音输入、中英文切换、候选词展示、系统托盘等功能。

## Glossary

- **TSF (Text Services Framework)**: Windows 文本服务框架，用于实现输入法
- **TIP (Text Input Processor)**: TSF 中的输入处理器，输入法的核心组件
- **ITfTextInputProcessor**: TSF 输入处理器接口，输入法必须实现
- **ITfKeyEventSink**: TSF 键盘事件接收器接口
- **ITfCompositionSink**: TSF 输入组合接收器接口
- **ITfContext**: TSF 上下文接口，用于与应用交互
- **ITfThreadMgr**: TSF 线程管理器，管理输入法生命周期
- **Composition**: 输入组合，即正在输入但未提交的文本
- **Preedit**: 预编辑文本，显示在应用中的输入中拼音
- **InputEngine**: 素言核心输入引擎，处理按键和候选词
- **IPlatformBridge**: 平台桥接接口，定义平台层需要实现的功能
- **CandidateWindow**: Qt 候选词窗口，跨平台 UI 组件
- **RIME**: 中州韵输入法引擎，提供拼音输入能力
- **System_Tray**: Windows 系统托盘，显示输入法状态图标

## Requirements

### Requirement 1: TSF 框架集成

**User Story:** 作为 Windows 用户，我希望素言输入法能够正确注册为系统输入法，以便在任何应用中使用。

#### Acceptance Criteria

1. THE TSF_Bridge SHALL implement ITfTextInputProcessor interface for system registration
2. THE TSF_Bridge SHALL implement ITfKeyEventSink interface for keyboard event handling
3. THE TSF_Bridge SHALL implement ITfCompositionSink interface for composition management
4. WHEN the input method is installed, THE TSF_Bridge SHALL register with Windows input method system
5. WHEN the input method is uninstalled, THE TSF_Bridge SHALL properly unregister and clean up resources
6. THE TSF_Bridge SHALL support Windows 10 version 1903 and later
7. THE TSF_Bridge SHALL support Windows 11

### Requirement 2: 键盘事件处理

**User Story:** 作为用户，我希望输入法能够正确接收和处理键盘输入，以便进行拼音输入。

#### Acceptance Criteria

1. WHEN a key is pressed, THE TSF_Bridge SHALL convert Windows virtual key code to RIME key code
2. WHEN a key is pressed, THE TSF_Bridge SHALL convert Windows modifier flags to RIME modifier mask
3. WHEN a key event is received, THE TSF_Bridge SHALL forward it to InputEngine for processing
4. WHEN InputEngine returns true, THE TSF_Bridge SHALL consume the key event
5. WHEN InputEngine returns false, THE TSF_Bridge SHALL pass the key event to the application
6. THE TSF_Bridge SHALL handle key down events for input processing
7. THE TSF_Bridge SHALL handle key up events for Shift key mode switching

### Requirement 3: 文本提交

**User Story:** 作为用户，我希望选择的候选词能够正确输入到当前应用中。

#### Acceptance Criteria

1. WHEN a candidate is selected, THE Windows_Bridge SHALL commit the text through ITfContext
2. WHEN committing text, THE Windows_Bridge SHALL use UTF-16 encoding for Windows compatibility
3. WHEN committing text, THE Windows_Bridge SHALL properly end the current composition
4. THE Windows_Bridge SHALL implement IPlatformBridge::commitText method
5. IF the target application does not support TSF, THEN THE Windows_Bridge SHALL fall back to SendInput

### Requirement 4: Preedit 显示

**User Story:** 作为用户，我希望正在输入的拼音能够显示在应用的光标位置。

#### Acceptance Criteria

1. WHEN input is in progress, THE Windows_Bridge SHALL update preedit text through ITfContext
2. WHEN preedit is updated, THE Windows_Bridge SHALL set correct caret position
3. WHEN input is cancelled, THE Windows_Bridge SHALL clear preedit text
4. THE Windows_Bridge SHALL implement IPlatformBridge::updatePreedit method
5. THE Windows_Bridge SHALL implement IPlatformBridge::clearPreedit method

### Requirement 5: 光标位置获取

**User Story:** 作为用户，我希望候选词窗口能够显示在光标附近的正确位置。

#### Acceptance Criteria

1. THE Windows_Bridge SHALL implement IPlatformBridge::getCursorPosition method
2. WHEN getting cursor position, THE Windows_Bridge SHALL query ITfContextView for text extent
3. WHEN cursor position is unavailable, THE Windows_Bridge SHALL return a reasonable default position
4. THE Windows_Bridge SHALL return screen coordinates compatible with Qt coordinate system

### Requirement 6: 候选词窗口显示

**User Story:** 作为用户，我希望候选词窗口能够正确显示并始终可见。

#### Acceptance Criteria

1. THE Candidate_Window SHALL use Qt framework for cross-platform compatibility
2. THE Candidate_Window SHALL be set as HWND_TOPMOST to stay above other windows
3. WHEN candidates are updated, THE Candidate_Window SHALL display at cursor position
4. WHEN no candidates exist, THE Candidate_Window SHALL hide automatically
5. THE Candidate_Window SHALL support horizontal and vertical layout modes
6. THE Candidate_Window SHALL support theme customization
7. WHEN the cursor moves, THE Candidate_Window SHALL follow the new position

### Requirement 7: 中英文模式切换

**User Story:** 作为用户，我希望能够方便地在中英文模式之间切换。

#### Acceptance Criteria

1. WHEN Shift key is pressed and released without other keys, THE InputEngine SHALL toggle between Chinese and English mode
2. WHEN in English mode, THE InputEngine SHALL pass all keys directly to the application
3. WHEN a capital letter is typed in Chinese mode, THE InputEngine SHALL enter temporary English mode
4. WHEN temporary English input is committed, THE InputEngine SHALL return to Chinese mode
5. THE System_Tray SHALL display current input mode icon

### Requirement 8: 候选词选择和翻页

**User Story:** 作为用户，我希望能够方便地选择候选词和浏览更多候选词。

#### Acceptance Criteria

1. WHEN a digit key (1-9) is pressed, THE InputEngine SHALL select the corresponding candidate
2. WHEN Space key is pressed, THE InputEngine SHALL commit the first candidate
3. WHEN Enter key is pressed, THE InputEngine SHALL commit the raw pinyin input
4. WHEN Page Down or Equal key is pressed, THE InputEngine SHALL show next page of candidates
5. WHEN Page Up or Minus key is pressed, THE InputEngine SHALL show previous page of candidates
6. WHEN Backspace is pressed, THE InputEngine SHALL delete the last input character
7. WHEN Escape is pressed, THE InputEngine SHALL clear all input and hide candidate window
8. WHEN Down arrow is pressed, THE InputEngine SHALL expand candidate rows (Sogou style)
9. WHEN in expanded mode, THE InputEngine SHALL support arrow key navigation between candidates

### Requirement 9: 系统托盘集成

**User Story:** 作为用户，我希望能够通过系统托盘查看和控制输入法状态。

#### Acceptance Criteria

1. THE Tray_Manager SHALL display an icon in Windows system tray
2. THE Tray_Manager SHALL show Chinese mode icon when in Chinese mode
3. THE Tray_Manager SHALL show English mode icon when in English mode
4. WHEN the tray icon is right-clicked, THE Tray_Manager SHALL show a context menu
5. THE context menu SHALL include option to switch input mode
6. THE context menu SHALL include option to open settings
7. THE context menu SHALL include option to show about dialog

### Requirement 10: 应用程序识别

**User Story:** 作为开发者，我希望能够获取当前应用信息以支持应用特定配置。

#### Acceptance Criteria

1. THE Windows_Bridge SHALL implement IPlatformBridge::getCurrentAppId method
2. WHEN getting app id, THE Windows_Bridge SHALL return the process executable name
3. IF app id cannot be determined, THEN THE Windows_Bridge SHALL return an empty string

### Requirement 11: 安装和部署

**User Story:** 作为用户，我希望能够方便地安装和卸载输入法。

#### Acceptance Criteria

1. THE Installer SHALL register the input method with Windows TSF system
2. THE Installer SHALL copy necessary files to appropriate locations
3. THE Installer SHALL register COM components for TSF
4. WHEN uninstalling, THE Installer SHALL properly unregister all components
5. WHEN uninstalling, THE Installer SHALL clean up user data optionally
6. THE Installer SHALL support silent installation mode
7. THE Installer SHALL support Windows 10 and Windows 11

### Requirement 12: Qt 与 TSF 集成

**User Story:** 作为开发者，我希望 Qt 事件循环能够与 TSF 消息循环正确协作。

#### Acceptance Criteria

1. THE Main_Entry SHALL initialize Qt application before TSF components
2. THE Main_Entry SHALL ensure Qt event loop processes Windows messages
3. THE Main_Entry SHALL properly clean up resources on shutdown
4. WHEN TSF callbacks occur, THE TSF_Bridge SHALL safely interact with Qt components
5. THE TSF_Bridge SHALL handle thread safety between TSF and Qt

### Requirement 13: RIME 引擎集成

**User Story:** 作为用户，我希望输入法使用与 macOS 相同的 RIME 引擎以获得一致的输入体验。

#### Acceptance Criteria

1. THE Main_Entry SHALL initialize RimeWrapper with correct data directories
2. THE InputEngine SHALL use RimeWrapper for pinyin processing
3. THE InputEngine SHALL use RimeWrapper for candidate generation
4. WHEN RIME initialization fails, THE Main_Entry SHALL display error message and exit gracefully
