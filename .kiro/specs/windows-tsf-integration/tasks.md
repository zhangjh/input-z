# Implementation Plan: Windows TSF Integration

## Overview

本实现计划将素言输入法移植到 Windows 平台，通过 TSF (Text Services Framework) 与系统集成。实现复用现有的跨平台核心层和 UI 层，仅需实现 Windows 平台特定的桥接代码。

## Tasks

- [x] 1. 项目结构和构建配置
  - [x] 1.1 创建 Windows 平台目录结构
    - 创建 `src/platform/windows/` 目录
    - 创建 CMakeLists.txt 配置文件
    - 配置 TSF SDK 依赖和链接
    - 配置 Windows 特定编译选项
    - _Requirements: 1.1, 1.6, 1.7_
  
  - [x] 1.2 创建 Windows 资源文件
    - 创建 resource.rc 资源定义
    - 添加版本信息和图标资源
    - 配置 DLL 清单文件
    - _Requirements: 11.2_

  - [x] 1.3 下载 Windows 版 librime 预编译库
    - 从 librime releases 下载 Windows 预编译包
    - 解压到 deps/librime-windows/
    - 更新 CMakeLists.txt 配置 Windows 平台链接
    - _Requirements: 13.1_

- [x] 2. 键码转换模块
  - [x] 2.1 实现键码转换函数
    - 创建 `key_converter.h/cpp`
    - 实现 convertVirtualKeyToRime 函数（VK_* 到 RIME 键码）
    - 实现 convertModifiersToRime 函数（修饰键状态转换）
    - 实现字符键检测和字符获取函数
    - _Requirements: 2.1, 2.2_

  - [x] 2.2 编写键码转换单元测试
    - 测试字母键、数字键、特殊键的转换
    - 测试修饰键组合的转换
    - **Property 1: 键码转换正确性**
    - **Property 2: 修饰键转换正确性**
    - _Validates: Requirements 2.1, 2.2_

- [x] 3. WindowsBridge 实现
  - [x] 3.1 实现 WindowsBridge 基础框架
    - 创建 `windows_bridge.h/cpp`
    - 实现 IPlatformBridge 接口声明
    - 实现 UTF-8/UTF-16 编码转换函数（utf8ToWide, wideToUtf8）
    - _Requirements: 3.2, 3.4, 4.4, 4.5, 5.1, 10.1_
  
  - [x] 3.2 编写编码转换单元测试
    - 测试中文、英文、混合文本的转换
    - **Property 4: UTF-8/UTF-16 编码转换往返一致性**
    - _Validates: Requirements 3.2_
  
  - [x] 3.3 实现 commitText 方法
    - 通过 TSF ITfContext 提交文本
    - 实现 SendInput 回退方案（用于不支持 TSF 的应用）
    - _Requirements: 3.1, 3.3, 3.5_
  
  - [x] 3.4 实现 preedit 相关方法
    - 实现 updatePreedit 方法（通过 TSF composition 更新）
    - 实现 clearPreedit 方法
    - _Requirements: 4.1, 4.2, 4.3_
  
  - [x] 3.5 实现 getCursorPosition 方法
    - 通过 ITfContextView::GetTextExt 获取光标位置
    - 处理获取失败时返回默认位置
    - 转换为 Qt 坐标系
    - _Requirements: 5.2, 5.3, 5.4_
  
  - [x] 3.6 实现 getCurrentAppId 方法
    - 获取前台窗口句柄
    - 获取进程 ID 和进程名
    - 处理获取失败返回空字符串
    - _Requirements: 10.2, 10.3_

  - [x] 3.7 编写 WindowsBridge 单元测试
    - 测试进程名获取格式
    - **Property 6: 进程名格式正确性**
    - _Validates: Requirements 10.2_

- [x] 4. TSFBridge COM 基础实现
  - [x] 4.1 定义 GUID 和 CLSID
    - 创建 `tsf_bridge.h`
    - 定义 CLSID_SuYanTextService
    - 定义 GUID_SuYanProfile
    - 定义语言配置 LANGID (0x0804 简体中文)
    - _Requirements: 1.1_
  
  - [x] 4.2 实现 IUnknown 接口
    - 创建 `tsf_bridge.cpp`
    - 实现 QueryInterface（支持所有必需接口）
    - 实现 AddRef/Release（引用计数管理）
    - _Requirements: 1.1_
  
  - [x] 4.3 实现 IClassFactory
    - 创建 TSFBridgeFactory 类
    - 实现 CreateInstance 方法
    - 实现 LockServer 方法
    - _Requirements: 1.1_
  
  - [x] 4.4 实现 DLL 导出函数
    - 实现 DllGetClassObject
    - 实现 DllCanUnloadNow
    - 实现 DllRegisterServer（注册 COM 和 TSF）
    - 实现 DllUnregisterServer（注销 COM 和 TSF）
    - _Requirements: 1.4, 1.5, 11.3_

- [x] 5. TSFBridge 核心接口实现
  - [x] 5.1 实现 ITfTextInputProcessor
    - 实现 Activate 方法
      - 保存 ITfThreadMgr 和 TfClientId
      - 初始化键盘事件接收器
      - 初始化 InputEngine 和 UI 组件
    - 实现 Deactivate 方法
      - 清理键盘事件接收器
      - 清理 composition
      - 释放 TSF 资源
    - _Requirements: 1.1, 1.5_
  
  - [x] 5.2 实现 ITfKeyEventSink
    - 实现 OnSetFocus 方法（处理焦点变化）
    - 实现 OnTestKeyDown/OnTestKeyUp 方法（预测试按键）
    - 实现 OnKeyDown 方法
      - 转换键码
      - 调用 InputEngine::processKeyEvent
      - 根据返回值设置 pfEaten
      - 更新候选词窗口位置
    - 实现 OnKeyUp 方法（处理 Shift 键释放切换模式）
    - 实现 OnPreservedKey 方法
    - _Requirements: 1.2, 2.3, 2.4, 2.5, 2.6, 2.7_

  - [x] 5.3 编写键事件处理单元测试
    - 测试 InputEngine 返回值与 pfEaten 的一致性
    - **Property 3: 键事件消费一致性**
    - _Validates: Requirements 2.4, 2.5_
  
  - [x] 5.4 实现 ITfCompositionSink
    - 实现 OnCompositionTerminated 方法（处理外部终止）
    - 实现 startComposition 辅助方法
    - 实现 endComposition 辅助方法
    - _Requirements: 1.3, 3.3_
  
  - [x] 5.5 实现 ITfDisplayAttributeProvider（可选）
    - 实现 EnumDisplayAttributeInfo 方法
    - 实现 GetDisplayAttributeInfo 方法
    - 配置 preedit 下划线样式
    - _Requirements: 4.1_

- [ ] 6. Checkpoint - TSFBridge 基础功能验证
  - 编译生成 DLL
  - 使用 regsvr32 注册 DLL
  - 验证输入法出现在系统输入法列表
  - 验证键盘事件能够接收
  - 如有问题请询问用户

- [ ] 7. TrayManager 实现
  - [ ] 7.1 实现 TrayManager 基础框架
    - 创建 `tray_manager.h/cpp`
    - 实现单例模式
    - 创建 QSystemTrayIcon 实例
    - 实现 initialize/shutdown 方法
    - _Requirements: 9.1_
  
  - [ ] 7.2 实现托盘菜单
    - 创建 QMenu 右键菜单
    - 添加"切换中/英文"选项
    - 添加"设置..."选项（暂时禁用）
    - 添加分隔线
    - 添加"关于素言"选项
    - 添加"退出"选项
    - 连接信号槽
    - _Requirements: 9.4, 9.5, 9.6, 9.7_
  
  - [ ] 7.3 实现图标更新
    - 加载中文模式图标
    - 加载英文模式图标
    - 实现 updateIcon(InputMode) 方法
    - _Requirements: 9.2, 9.3, 7.5_

- [ ] 8. 主程序入口
  - [ ] 8.1 实现 main.cpp 基础框架
    - 创建 `main.cpp`
    - 实现 WinMain 入口点
    - 初始化 COM (CoInitializeEx)
    - 初始化 Qt 应用 (QApplication)
    - _Requirements: 12.1, 12.2_
  
  - [ ] 8.2 实现组件初始化
    - 获取数据目录路径（AppData/Roaming/SuYan）
    - 初始化 RimeWrapper
    - 初始化 InputEngine
    - 初始化 CandidateWindow
    - 初始化 TrayManager
    - 创建 WindowsBridge 并连接到 InputEngine
    - _Requirements: 13.1, 13.2, 13.3_
  
  - [ ] 8.3 实现组件连接
    - 设置 InputEngine 状态变化回调到 CandidateWindow
    - 设置 InputEngine 提交文本回调到 WindowsBridge
    - 连接 TrayManager 信号到 InputEngine
    - _Requirements: 12.4_
  
  - [ ] 8.4 实现清理和错误处理
    - 实现资源清理函数
    - 处理 RIME 初始化失败（显示错误对话框）
    - 处理 Qt 初始化失败
    - _Requirements: 12.3, 13.4_

- [ ] 9. 候选词窗口 Windows 适配
  - [ ] 9.1 实现 Windows 窗口置顶
    - 创建 `candidate_window_win.cpp`
    - 获取 Qt 窗口的 HWND
    - 设置 HWND_TOPMOST 标志
    - 设置 WS_EX_NOACTIVATE 防止获取焦点
    - _Requirements: 6.2_
  
  - [ ] 9.2 处理全屏应用显示
    - 检测全屏应用
    - 确保候选词窗口在全屏应用中可见
    - _Requirements: 6.2_
  
  - [ ] 9.3 验证跨平台 UI 复用
    - 确认 CandidateWindow 正常显示
    - 确认横排/竖排布局切换正常
    - 确认主题切换正常
    - 确认方向键展开和导航正常
    - _Requirements: 6.1, 6.5, 6.6, 6.7_

  - [ ]* 9.4 编写候选词窗口单元测试
    - 测试空候选词时窗口隐藏
    - **Property 5: 空候选词窗口自动隐藏**
    - _Validates: Requirements 6.4_

- [ ] 10. Checkpoint - 完整功能集成测试
  - 测试拼音输入完整流程
  - 测试候选词选择（数字键、空格、回车）
  - 测试翻页（PageUp/PageDown、+/-）
  - 测试方向键展开和导航
  - 测试中英文切换（Shift 键）
  - 测试临时英文模式（大写字母开头）
  - 测试系统托盘功能
  - 在多个应用中测试（记事本、浏览器、VS Code）
  - 如有问题请询问用户

- [ ] 11. 安装程序
  - [ ] 11.1 创建 NSIS 安装脚本
    - 创建 `installer/suyan.nsi`
    - 配置安装目录（Program Files/SuYan）
    - 配置文件复制（DLL、RIME 数据、主题、图标）
    - 配置 COM 注册（regsvr32）
    - 配置开始菜单快捷方式
    - _Requirements: 11.1, 11.2, 11.3_
  
  - [ ] 11.2 实现卸载功能
    - 注销 COM 组件
    - 删除安装文件
    - 删除开始菜单快捷方式
    - 可选清理用户数据（AppData/Roaming/SuYan）
    - _Requirements: 11.4, 11.5_
  
  - [ ] 11.3 配置静默安装
    - 支持 /S 参数静默安装
    - 支持 /D 参数指定安装目录
    - _Requirements: 11.6_

- [ ] 12. Final Checkpoint - 完整验收测试
  - 测试全新安装流程
  - 测试卸载流程
  - 在 Windows 10 上验证所有功能
  - 在 Windows 11 上验证所有功能
  - 验证与 macOS 版本功能一致性
  - 记录已知问题和限制
  - 如有问题请询问用户

## Notes

- 带 `*` 标记的任务为可选测试任务
- 每个任务引用具体的需求编号以保证可追溯性
- Checkpoint 任务用于阶段性验证，确保增量开发的正确性
- Windows 平台复用现有的核心层（InputEngine、RimeWrapper）和 UI 层（CandidateWindow、ThemeManager、LayoutManager）
- TSF 实现参考 macOS IMK 实现的架构模式
