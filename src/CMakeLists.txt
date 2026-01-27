# SuYan Source - CMake 配置

# 核心层
add_subdirectory(core)

# UI 层
add_subdirectory(ui)

# 平台层
if(APPLE)
    add_subdirectory(platform/macos)
elseif(WIN32)
    add_subdirectory(platform/windows)
    add_subdirectory(server)
endif()
