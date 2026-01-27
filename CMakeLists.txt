# SuYan Input Method - CMake 配置
cmake_minimum_required(VERSION 3.20)

# 平台特定语言配置
if(APPLE)
    set(SUYAN_LANGUAGES CXX C OBJCXX)
elseif(WIN32)
    set(SUYAN_LANGUAGES CXX C)
else()
    set(SUYAN_LANGUAGES CXX C)
endif()

project(SuYan
    VERSION 1.0.0
    DESCRIPTION "Cross-platform Chinese Input Method"
    LANGUAGES ${SUYAN_LANGUAGES}
)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# 平台特定配置
# ============================================

if(APPLE)
    # macOS 部署目标
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
    
    # Qt 配置 - Homebrew Qt 路径（Apple Silicon）
    set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6" CACHE PATH "Qt6 CMake directory")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qt")
    
    # librime 配置 (macOS)
    set(LIBRIME_DIR "${CMAKE_SOURCE_DIR}/deps/librime")
    set(LIBRIME_INCLUDE_DIR "${LIBRIME_DIR}/include")
    set(LIBRIME_LIB_DIR "${LIBRIME_DIR}/lib")
    
    # 创建 librime imported target (macOS)
    add_library(librime SHARED IMPORTED)
    set_target_properties(librime PROPERTIES
        IMPORTED_LOCATION "${LIBRIME_LIB_DIR}/librime.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBRIME_INCLUDE_DIR}"
    )
    
elseif(WIN32)
    # Windows 平台配置
    
    # librime 配置 (Windows)
    set(LIBRIME_DIR "${CMAKE_SOURCE_DIR}/deps/librime")
    set(LIBRIME_INCLUDE_DIR "${LIBRIME_DIR}/include")
    
    # 根据目标架构选择库目录
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(LIBRIME_LIB_DIR "${LIBRIME_DIR}/lib/x64")
        set(SUYAN_ARCH "x64")
        set(SUYAN_BUILD_FULL ON)
    else()
        set(LIBRIME_LIB_DIR "${LIBRIME_DIR}/lib/x86")
        set(SUYAN_ARCH "x86")
        set(SUYAN_BUILD_FULL OFF)
    endif()
    
    message(STATUS "Target architecture: ${SUYAN_ARCH}")
    message(STATUS "librime library: ${LIBRIME_LIB_DIR}")
    
    # 32位只构建轻量级TSF客户端，不需要Qt和librime
    if(NOT SUYAN_BUILD_FULL)
        message(STATUS "Building 32-bit TSF client only (no Qt/RIME)")
        add_subdirectory(src/platform/windows/tsf_client)
        return()
    endif()
    
    # 64位完整版本需要librime
    add_library(librime SHARED IMPORTED)
    set_target_properties(librime PROPERTIES
        IMPORTED_LOCATION "${LIBRIME_LIB_DIR}/rime.dll"
        IMPORTED_IMPLIB "${LIBRIME_LIB_DIR}/rime.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBRIME_INCLUDE_DIR}"
    )
endif()

# 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

# Qt MOC/UIC/RCC 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加子目录
add_subdirectory(src)
add_subdirectory(tests)
