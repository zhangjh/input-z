# SuYan Source - CMake 配置

# 核心层
add_subdirectory(core)

# UI 层
add_subdirectory(ui)

# 剪贴板模块
add_subdirectory(clipboard)

# 平台层
if(APPLE)
    add_subdirectory(platform/macos)
endif()

# Windows 平台层（Phase 2）
# if(WIN32)
#     add_subdirectory(platform/windows)
# endif()
