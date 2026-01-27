# SuYan UI Layer - CMake 配置

# 查找 yaml-cpp
find_package(yaml-cpp REQUIRED)

# UI 层源文件
set(UI_SOURCES
    theme_manager.cpp
    layout_manager.cpp
    candidate_view.cpp
    candidate_window.cpp
    suyan_ui_init.cpp
)

set(UI_HEADERS
    theme_manager.h
    layout_manager.h
    candidate_view.h
    candidate_window.h
    suyan_ui_init.h
)

# macOS 特定源文件
if(APPLE)
    list(APPEND UI_SOURCES candidate_window_mac.mm)
endif()

# Windows 特定源文件
if(WIN32)
    list(APPEND UI_SOURCES candidate_window_win.cpp)
endif()

# 创建 UI 层静态库
add_library(suyan_ui STATIC
    ${UI_SOURCES}
    ${UI_HEADERS}
)

# 包含目录
target_include_directories(suyan_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖
target_link_libraries(suyan_ui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    yaml-cpp::yaml-cpp
    suyan_core
)

# macOS 特定链接
if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(suyan_ui PUBLIC
        ${COCOA_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
    )
endif()

# Windows 特定链接
if(WIN32)
    target_link_libraries(suyan_ui PUBLIC
        dwmapi
    )
endif()

# 如果没有源文件，添加一个空的占位
# if(NOT UI_SOURCES)
#     set_target_properties(suyan_ui PROPERTIES LINKER_LANGUAGE CXX)
# endif()
