# SuYan Windows Platform Layer - CMake 配置
# 新架构：DLL 是纯 Win32 IPC 客户端，无 Qt/RIME 依赖

if(NOT WIN32)
    return()
endif()

# ============================================
# Windows SDK 配置
# ============================================

set(CMAKE_SYSTEM_VERSION "10.0.18362.0" CACHE STRING "Minimum Windows SDK version")

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
)

if(MSVC)
    add_compile_options(/utf-8 /W4 /WX- /MP /Zc:__cplusplus)
    add_link_options(/SUBSYSTEM:WINDOWS)
endif()

# ============================================
# 资源路径
# ============================================

set(WINDOWS_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/app-icon.ico")
set(RIME_DATA_DIR "${CMAKE_SOURCE_DIR}/data/rime")
set(THEME_DIR "${CMAKE_SOURCE_DIR}/resources/themes")

# ============================================
# 纯 Win32 TSF DLL（无 Qt/RIME 依赖）
# ============================================

add_subdirectory(tsf_dll)

# ============================================
# 旧的 Windows Bridge 已移除
# 新架构中 Server 直接使用 suyan_core 和 suyan_ui
# DLL 是纯 Win32 IPC 客户端，不需要这些组件
# ============================================

# ============================================
# 资源复制（到 Server 输出目录）
# ============================================

add_custom_target(copy_windows_resources ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${RIME_DATA_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rime"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${THEME_DIR}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBRIME_LIB_DIR}/rime.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    COMMENT "Copying RIME data, themes, and librime DLL..."
)

# ============================================
# 注册脚本
# ============================================

# 注册脚本在 tsf_dll/CMakeLists.txt 中生成和复制

# ============================================
# 状态输出
# ============================================

message(STATUS "")
message(STATUS "=== Windows Platform Configuration ===")
message(STATUS "  Architecture: ${SUYAN_ARCH}")
message(STATUS "  TSF DLL: Pure Win32 (no Qt/RIME)")
message(STATUS "  TSF DLL output: SuYan.dll (x64), SuYan32.dll (x86)")
message(STATUS "  librime: ${LIBRIME_LIB_DIR}")
message(STATUS "")
