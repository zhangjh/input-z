# SuYan Tests - CMake 配置

# MSVC 需要 /utf-8 选项来正确处理 UTF-8 源文件
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt 环境验证测试
add_executable(qt_env_test qt_env_test.cpp)
target_link_libraries(qt_env_test PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# librime API 验证测试
add_executable(librime_test librime_test.cpp)
target_include_directories(librime_test PRIVATE ${LIBRIME_INCLUDE_DIR})
target_link_libraries(librime_test PRIVATE librime)

# 设置 RPATH 以便运行时找到 librime
set_target_properties(librime_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ========== 核心层单元测试 ==========

# RimeWrapper 单元测试
add_executable(rime_wrapper_test core/rime_wrapper_test.cpp)
target_link_libraries(rime_wrapper_test PRIVATE suyan_core)
set_target_properties(rime_wrapper_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# InputEngine 单元测试
add_executable(input_engine_test core/input_engine_test.cpp)
target_link_libraries(input_engine_test PRIVATE suyan_core)
set_target_properties(input_engine_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ConfigManager 单元测试
add_executable(config_manager_test core/config_manager_test.cpp)
target_link_libraries(config_manager_test PRIVATE suyan_core Qt6::Test)
set_target_properties(config_manager_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# FrequencyManager 单元测试
add_executable(frequency_manager_test core/frequency_manager_test.cpp)
target_link_libraries(frequency_manager_test PRIVATE suyan_core Qt6::Test)
set_target_properties(frequency_manager_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ========== UI 层测试 ==========

# UI 测试程序
add_executable(ui_test_app ui/ui_test_app.cpp)
target_link_libraries(ui_test_app PRIVATE 
    suyan_ui 
    suyan_core 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
)
set_target_properties(ui_test_app PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
    AUTOMOC ON
)

# Qt/IMK 集成测试
add_executable(qt_imk_integration_test ui/qt_imk_integration_test.mm)
target_link_libraries(qt_imk_integration_test PRIVATE 
    suyan_ui 
    suyan_core 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
)
set_target_properties(qt_imk_integration_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# macOS 特定：链接 Cocoa 框架
if(APPLE)
    target_link_libraries(qt_imk_integration_test PRIVATE
        "-framework Cocoa"
        "-framework AppKit"
    )
endif()

# 候选词窗口可见性测试
# Task 9.4: 编写候选词窗口单元测试
# Property 5: 空候选词窗口自动隐藏
# Validates: Requirements 6.4
add_executable(candidate_window_visibility_test ui/candidate_window_visibility_test.cpp)
target_link_libraries(candidate_window_visibility_test PRIVATE 
    suyan_ui 
    suyan_core 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets
)
set_target_properties(candidate_window_visibility_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
    AUTOMOC ON
)

# ========== Windows 平台测试 ==========

# 键码转换单元测试
# Task 2.2: 编写键码转换单元测试
# Property 1: 键码转换正确性
# Property 2: 修饰键转换正确性
# Validates: Requirements 2.1, 2.2
# 
# 注意：此测试使用模拟的 Windows API，可在所有平台编译运行
add_executable(key_converter_test platform/windows/key_converter_test.cpp)
target_include_directories(key_converter_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/platform/windows
)
# MSVC 需要 /utf-8 选项来正确处理 UTF-8 源文件
if(MSVC)
    target_compile_options(key_converter_test PRIVATE /utf-8)
endif()
# 此测试不依赖其他库，使用内置的模拟实现

# 编码转换单元测试
# Task 3.2: 编写编码转换单元测试
# Property 4: UTF-8/UTF-16 编码转换往返一致性
# Validates: Requirements 3.2
#
# 注意：此测试在非 Windows 平台使用标准库 codecvt 实现
add_executable(windows_bridge_encoding_test platform/windows/windows_bridge_encoding_test.cpp)
target_include_directories(windows_bridge_encoding_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/platform/windows
)
# MSVC 需要 /utf-8 选项来正确处理 UTF-8 源文件
if(MSVC)
    target_compile_options(windows_bridge_encoding_test PRIVATE /utf-8)
endif()
# 此测试不依赖其他库，使用内置的编码转换实现

# WindowsBridge 进程名获取单元测试
# Task 3.7: 编写 WindowsBridge 单元测试
# Property 6: 进程名格式正确性
# Validates: Requirements 10.2
#
# 注意：此测试在非 Windows 平台使用模拟实现
add_executable(windows_bridge_process_test platform/windows/windows_bridge_process_test.cpp)
target_include_directories(windows_bridge_process_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/platform/windows
)
# MSVC 需要 /utf-8 选项来正确处理 UTF-8 源文件
if(MSVC)
    target_compile_options(windows_bridge_process_test PRIVATE /utf-8)
endif()
# 此测试不依赖其他库，使用内置的模拟实现


# 光标位置获取测试 (Windows only)
if(WIN32)
    add_executable(cursor_position_test platform/windows/cursor_position_test.cpp)
    target_link_libraries(cursor_position_test PRIVATE imm32)
    if(MSVC)
        target_compile_options(cursor_position_test PRIVATE /utf-8)
    endif()
endif()
