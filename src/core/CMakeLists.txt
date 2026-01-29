# SuYan Core Layer - CMake 配置

# 查找 yaml-cpp
find_package(yaml-cpp REQUIRED)

# 查找 SQLite3
find_package(SQLite3 REQUIRED)

# 核心层源文件
set(CORE_SOURCES
    rime_wrapper.cpp
    input_engine.cpp
    config_manager.cpp
    frequency_manager.cpp
)

set(CORE_HEADERS
    rime_wrapper.h
    platform_bridge.h
    input_engine.h
    config_manager.h
    frequency_manager.h
)

# Windows IPC
if(WIN32)
    list(APPEND CORE_SOURCES ipc_channel.cpp)
    list(APPEND CORE_HEADERS ipc_channel.h)
endif()

# 创建核心层静态库
add_library(suyan_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

# 包含目录
target_include_directories(suyan_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/shared
    ${LIBRIME_INCLUDE_DIR}
)

# 链接依赖
target_link_libraries(suyan_core PUBLIC
    Qt6::Core
    librime
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
)
