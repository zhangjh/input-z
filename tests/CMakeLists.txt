# SuYan Tests - CMake 配置

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

# ========== 剪贴板模块单元测试 ==========

# ClipboardStore 单元测试
add_executable(clipboard_store_test clipboard/clipboard_store_test.cpp)
target_link_libraries(clipboard_store_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Test
)
set_target_properties(clipboard_store_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ImageStorage 单元测试
add_executable(image_storage_test clipboard/image_storage_test.cpp)
target_link_libraries(image_storage_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Gui
    Qt6::Test
)
set_target_properties(image_storage_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# 存储层功能验证测试（Task 5 Checkpoint）
add_executable(storage_validation_test clipboard/storage_validation_test.cpp)
target_link_libraries(storage_validation_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Gui
    Qt6::Test
)
set_target_properties(storage_validation_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ClipboardManager 单元测试
add_executable(clipboard_manager_test clipboard/clipboard_manager_test.cpp)
target_link_libraries(clipboard_manager_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Gui
    Qt6::Test
)
set_target_properties(clipboard_manager_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
)

# ClipboardManager 集成测试（Task 9 Checkpoint）
add_executable(clipboard_integration_test clipboard/clipboard_integration_test.cpp)
target_link_libraries(clipboard_integration_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Gui
    Qt6::Test
)
set_target_properties(clipboard_integration_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
    AUTOMOC ON
)

# ClipboardWindow UI 层测试（Task 13 Checkpoint）
add_executable(clipboard_ui_test clipboard/clipboard_ui_test.cpp)
target_link_libraries(clipboard_ui_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
set_target_properties(clipboard_ui_test PROPERTIES
    BUILD_RPATH "${LIBRIME_LIB_DIR}"
    INSTALL_RPATH "${LIBRIME_LIB_DIR}"
    AUTOMOC ON
)

# 剪贴板性能测试（Task 20）
add_executable(clipboard_performance_test clipboard/performance_test.cpp)
target_link_libraries(clipboard_performance_test PRIVATE 
    suyan_clipboard 
    Qt6::Core 
    Qt6::Test
)
set_target_properties(clipboard_performance_test PROPERTIES
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
