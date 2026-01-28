# SuYan Clipboard Module - CMake 配置

# 查找 SQLite3
find_package(SQLite3 REQUIRED)

# 剪贴板模块源文件
set(CLIPBOARD_SOURCES
    clipboard_store.cpp
    image_storage.cpp
    hotkey_manager.cpp
    clipboard_manager.cpp
    # UI 层
    clipboard_item_widget.cpp
    clipboard_list.cpp
    clipboard_window.cpp
)

set(CLIPBOARD_HEADERS
    clipboard_store.h
    image_storage.h
    clipboard_monitor.h
    hotkey_manager.h
    clipboard_manager.h
    # UI 层
    clipboard_item_widget.h
    clipboard_list.h
    clipboard_window.h
)

# macOS 平台特定源文件
if(APPLE)
    set(CLIPBOARD_PLATFORM_SOURCES
        platform/mac_clipboard_monitor.mm
        platform/mac_hotkey_handler.mm
    )
    set(CLIPBOARD_PLATFORM_HEADERS
        platform/mac_clipboard_monitor.h
        platform/mac_hotkey_handler.h
    )
endif()

# Windows 平台特定源文件（Phase 2）
# if(WIN32)
#     set(CLIPBOARD_PLATFORM_SOURCES
#         platform/win_clipboard_monitor.cpp
#         platform/win_hotkey_handler.cpp
#     )
# endif()

# 创建剪贴板模块静态库
add_library(suyan_clipboard STATIC
    ${CLIPBOARD_SOURCES}
    ${CLIPBOARD_HEADERS}
    ${CLIPBOARD_PLATFORM_SOURCES}
    ${CLIPBOARD_PLATFORM_HEADERS}
)

# 包含目录
target_include_directories(suyan_clipboard PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖
target_link_libraries(suyan_clipboard PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    SQLite::SQLite3
)

# macOS 特定链接
if(APPLE)
    # AppKit 框架用于剪贴板和应用信息
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    # Carbon 框架用于全局快捷键
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    target_link_libraries(suyan_clipboard PUBLIC
        ${APPKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${CARBON_FRAMEWORK}
    )
endif()
