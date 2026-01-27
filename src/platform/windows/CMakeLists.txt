# SuYan Windows Platform Layer - CMake 配置

# 仅在 Windows 上编译
if(NOT WIN32)
    return()
endif()

# ============================================
# Windows SDK 和 TSF 配置
# ============================================

# 最低 Windows 版本要求 (Windows 10 1903)
set(CMAKE_SYSTEM_VERSION "10.0.18362.0" CACHE STRING "Minimum Windows SDK version")

# Windows 特定编译选项
add_compile_definitions(
    WIN32_LEAN_AND_MEAN      # 排除不常用的 Windows 头文件
    NOMINMAX                 # 禁用 Windows.h 中的 min/max 宏
    UNICODE                  # 使用 Unicode 版本的 Windows API
    _UNICODE
    _WIN32_WINNT=0x0A00      # Windows 10
    WINVER=0x0A00
)

# MSVC 特定选项
if(MSVC)
    add_compile_options(
        /utf-8                # 源文件和执行字符集都使用 UTF-8
        /W4                   # 警告级别 4
        /WX-                  # 不将警告视为错误（开发阶段）
        /MP                   # 多处理器编译
        /Zc:__cplusplus       # 正确报告 __cplusplus 宏
    )
    
    # 链接器选项
    add_link_options(
        /SUBSYSTEM:WINDOWS    # Windows 子系统（无控制台窗口）
    )
endif()

# ============================================
# 资源文件
# ============================================

# 图标资源
set(WINDOWS_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/app-icon.ico")

# RIME 数据目录
set(RIME_DATA_DIR "${CMAKE_SOURCE_DIR}/data/rime")

# 主题资源
set(THEME_DIR "${CMAKE_SOURCE_DIR}/resources/themes")

# librime 路径（从根CMakeLists.txt继承 LIBRIME_INCLUDE_DIR 和 LIBRIME_LIB_DIR）

# ============================================
# 源文件
# ============================================

set(WINDOWS_SOURCES
    main.cpp                # 程序入口
    tsf_bridge.cpp          # TSF 桥接实现
    windows_bridge.cpp      # WindowsBridge 实现
    key_converter.cpp       # 键码转换
    language_bar.cpp        # TSF 语言栏实现
)

set(WINDOWS_HEADERS
    tsf_bridge.h            # TSF 桥接头文件
    windows_bridge.h        # WindowsBridge 头文件
    key_converter.h         # 键码转换头文件
    language_bar.h          # TSF 语言栏头文件
    tsf_types.h             # TSF 类型定义
    registry_config.h       # 注册表配置
)

# 资源文件
set(WINDOWS_RESOURCES
    resource.rc             # Windows 资源定义
)

# ============================================
# Windows 系统库
# ============================================

# TSF 相关库
set(TSF_LIBRARIES
    ole32           # COM 基础
    oleaut32        # COM 自动化
    uuid            # GUID 定义
    advapi32        # 注册表操作
    user32          # 用户界面
    shell32         # Shell 功能
    shlwapi         # Shell 轻量级工具
)

# ============================================
# 创建 Object Library（用于编译检查和测试）
# ============================================

add_library(suyan_windows_bridge OBJECT
    ${WINDOWS_SOURCES}
    ${WINDOWS_HEADERS}
)

# 包含目录
target_include_directories(suyan_windows_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
    ${LIBRIME_INCLUDE_DIR}
)

# 链接依赖
target_link_libraries(suyan_windows_bridge PRIVATE
    suyan_core
    suyan_ui
    ${TSF_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ============================================
# TSF DLL 配置
# ============================================

# 创建 TSF 输入法 DLL
add_library(SuYanTSF SHARED
    ${WINDOWS_SOURCES}
    ${WINDOWS_HEADERS}
    ${WINDOWS_RESOURCES}
)

# DLL 属性配置
# 32位和64位使用不同的输出名称
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SUYAN_DLL_NAME "SuYan")
else()
    set(SUYAN_DLL_NAME "SuYan32")
endif()

set_target_properties(SuYanTSF PROPERTIES
    OUTPUT_NAME "${SUYAN_DLL_NAME}"
    PREFIX ""
    SUFFIX ".dll"
    
    # 版本信息
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    
    # 导出符号
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    
    # 运行时库
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# 包含目录
target_include_directories(SuYanTSF PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
    ${LIBRIME_INCLUDE_DIR}
)

# 链接依赖
target_link_libraries(SuYanTSF PRIVATE
    # 内部库
    suyan_ui
    suyan_core
    
    # Windows 系统库
    ${TSF_LIBRARIES}
    
    # Qt 库
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# DEF 文件（定义 DLL 导出函数）
set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/suyan.def")
if(EXISTS ${DEF_FILE})
    target_sources(SuYanTSF PRIVATE ${DEF_FILE})
endif()

# ============================================
# 资源文件复制
# ============================================

# 复制 RIME 数据到输出目录
add_custom_command(TARGET SuYanTSF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SuYanTSF>/rime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RIME_DATA_DIR}"
        "$<TARGET_FILE_DIR:SuYanTSF>/rime"
    COMMENT "Copying RIME data..."
)

# 复制主题到输出目录
add_custom_command(TARGET SuYanTSF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SuYanTSF>/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${THEME_DIR}"
        "$<TARGET_FILE_DIR:SuYanTSF>/themes"
    COMMENT "Copying themes..."
)

# 复制 librime DLL 到输出目录
add_custom_command(TARGET SuYanTSF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBRIME_LIB_DIR}/rime.dll"
        "$<TARGET_FILE_DIR:SuYanTSF>/"
    COMMENT "Copying librime DLL (${SUYAN_ARCH})..."
)

# ============================================
# Qt 部署
# ============================================

# 查找 windeployqt 工具
find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
          "$ENV{QTDIR}/bin"
)

if(WINDEPLOYQT_EXECUTABLE)
    message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
    
    # 添加 Qt 部署目标
    add_custom_target(deploy_qt_windows
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "$<TARGET_FILE:SuYanTSF>"
        DEPENDS SuYanTSF
        COMMENT "Deploying Qt dependencies..."
    )
else()
    message(WARNING "windeployqt not found, Qt DLLs will not be deployed automatically")
endif()

# ============================================
# 安装规则
# ============================================

# 安装 DLL 和依赖
install(TARGETS SuYanTSF
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
    COMPONENT Runtime
)

# 安装 RIME 数据
install(DIRECTORY "${RIME_DATA_DIR}/"
    DESTINATION "rime"
    COMPONENT Runtime
)

# 安装主题
install(DIRECTORY "${THEME_DIR}/"
    DESTINATION "themes"
    COMPONENT Runtime
)

# 安装 librime DLL
install(FILES "${LIBRIME_LIB_DIR}/rime.dll"
    DESTINATION "."
    COMPONENT Runtime
)

# ============================================
# 注册/注销脚本
# ============================================

# 创建注册脚本（同时注册64位和32位DLL）
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/register_admin.bat"
    "@echo off\n"
    "echo ========================================\n"
    "echo SuYan Input Method Registration\n"
    "echo ========================================\n"
    "echo.\n"
    "net session >nul 2>&1\n"
    "if %errorlevel% neq 0 (\n"
    "    echo ERROR: Please run this script as Administrator!\n"
    "    pause\n"
    "    exit /b 1\n"
    ")\n"
    "echo Registering 64-bit SuYan.dll...\n"
    "regsvr32 /s \"%~dp0SuYan.dll\"\n"
    "if %errorlevel% equ 0 ( echo   64-bit: OK ) else ( echo   64-bit: FAILED )\n"
    "if exist \"%~dp0SuYan32.dll\" (\n"
    "    echo Registering 32-bit SuYan32.dll...\n"
    "    regsvr32 /s \"%~dp0SuYan32.dll\"\n"
    "    if %errorlevel% equ 0 ( echo   32-bit: OK ) else ( echo   32-bit: FAILED )\n"
    ")\n"
    "echo.\n"
    "echo Registration complete.\n"
    "echo Add SuYan in: Settings ^> Time ^& Language ^> Language ^> Options ^> Add keyboard\n"
    "echo.\n"
    "pause\n"
)

# 创建注销脚本
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/unregister_admin.bat"
    "@echo off\n"
    "echo ========================================\n"
    "echo SuYan Input Method Unregistration\n"
    "echo ========================================\n"
    "echo.\n"
    "net session >nul 2>&1\n"
    "if %errorlevel% neq 0 (\n"
    "    echo ERROR: Please run this script as Administrator!\n"
    "    pause\n"
    "    exit /b 1\n"
    ")\n"
    "if exist \"%~dp0SuYan32.dll\" (\n"
    "    echo Unregistering 32-bit SuYan32.dll...\n"
    "    regsvr32 /u /s \"%~dp0SuYan32.dll\"\n"
    "    if %errorlevel% equ 0 ( echo   32-bit: OK ) else ( echo   32-bit: FAILED )\n"
    ")\n"
    "echo Unregistering 64-bit SuYan.dll...\n"
    "regsvr32 /u /s \"%~dp0SuYan.dll\"\n"
    "if %errorlevel% equ 0 ( echo   64-bit: OK ) else ( echo   64-bit: FAILED )\n"
    "echo.\n"
    "echo Unregistration complete.\n"
    "echo.\n"
    "pause\n"
)

# 复制注册脚本到输出目录
add_custom_command(TARGET SuYanTSF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/register_admin.bat"
        "$<TARGET_FILE_DIR:SuYanTSF>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/unregister_admin.bat"
        "$<TARGET_FILE_DIR:SuYanTSF>/"
    COMMENT "Copying registration scripts..."
)

# 安装注册脚本
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/register_admin.bat"
    "${CMAKE_CURRENT_BINARY_DIR}/unregister_admin.bat"
    DESTINATION "."
    COMPONENT Runtime
)

# ============================================
# 状态输出
# ============================================

message(STATUS "")
message(STATUS "=== Windows Platform Layer Configuration ===")
message(STATUS "  Windows SDK: ${CMAKE_SYSTEM_VERSION}")
message(STATUS "  Icon: ${WINDOWS_ICON_FILE}")
message(STATUS "  RIME data: ${RIME_DATA_DIR}")
message(STATUS "  Themes: ${THEME_DIR}")
message(STATUS "  librime: ${LIBRIME_LIB_DIR}")
message(STATUS "  windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
message(STATUS "")
message(STATUS "Build targets:")
message(STATUS "  SuYanTSF           - Build the TSF DLL")
message(STATUS "  deploy_qt_windows  - Deploy Qt dependencies")
message(STATUS "")
message(STATUS "After build:")
message(STATUS "  Run register.bat as Administrator to register the input method")
message(STATUS "")
