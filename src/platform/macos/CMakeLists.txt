# SuYan macOS Platform Layer - CMake 配置
# Task 10.1: 配置 CMake 安装规则

# 仅在 macOS 上编译
if(NOT APPLE)
    return()
endif()

# ============================================
# 资源文件
# ============================================

# 图标资源
set(MACOS_ICON_FILE "${CMAKE_SOURCE_DIR}/resources/icons/generated/SuYan.icns")

# RIME 数据目录
set(RIME_DATA_DIR "${CMAKE_SOURCE_DIR}/data/rime")

# 主题资源
set(THEME_DIR "${CMAKE_SOURCE_DIR}/resources/themes")

# librime 库文件
set(LIBRIME_DYLIB "${LIBRIME_LIB_DIR}/librime.dylib")
set(LIBRIME_DYLIB_VERSIONED "${LIBRIME_LIB_DIR}/librime.1.dylib")

# ============================================
# 源文件
# ============================================

set(MACOS_SOURCES
    main.mm             # Task 6.4: 输入法主入口
    IMKBridge.mm        # Task 6.2: IMK 桥接实现
    macos_bridge.mm     # Task 6.3: MacOSBridge 实现
    status_bar_manager.mm  # Task 8.2: 状态栏图标管理
)

set(MACOS_HEADERS
    IMKBridge.h         # Task 6.2: IMK 桥接头文件
    macos_bridge.h      # Task 6.3: MacOSBridge 头文件
    status_bar_manager.h   # Task 8.2: 状态栏图标管理头文件
)

# ============================================
# macOS 框架
# ============================================

find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
find_library(INPUTMETHODKIT_FRAMEWORK InputMethodKit REQUIRED)
find_library(CARBON_FRAMEWORK Carbon REQUIRED)

# ============================================
# 创建 Object Library（用于编译检查）
# ============================================

# 创建 object library 用于编译 IMK 桥接代码
# 这样可以在没有 main.mm 的情况下验证代码编译
add_library(suyan_macos_bridge OBJECT
    ${MACOS_SOURCES}
    ${MACOS_HEADERS}
)

# 包含目录
target_include_directories(suyan_macos_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/clipboard
    ${LIBRIME_INCLUDE_DIR}
)

# 链接依赖（object library 使用 INTERFACE）
target_link_libraries(suyan_macos_bridge PRIVATE
    suyan_core
    suyan_ui
    suyan_clipboard
    ${COCOA_FRAMEWORK}
    ${INPUTMETHODKIT_FRAMEWORK}
    ${CARBON_FRAMEWORK}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ============================================
# 输入法 Bundle 配置
# ============================================

# 创建输入法可执行文件（MACOSX_BUNDLE 生成 .app）
add_executable(SuYan MACOSX_BUNDLE
    ${MACOS_SOURCES}
    ${MACOS_HEADERS}
    ${MACOS_ICON_FILE}
)

# ============================================
# Bundle 属性配置
# ============================================

set_target_properties(SuYan PROPERTIES
    # 基本 Bundle 配置
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    
    # Bundle 元数据
    MACOSX_BUNDLE_BUNDLE_NAME "SuYan"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "cn.zhangjh.inputmethod.SuYan"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 zhangjh. All rights reserved."
    
    # 图标
    MACOSX_BUNDLE_ICON_FILE "SuYan"
    
    # 输出名称
    OUTPUT_NAME "SuYan"
    
    # RPATH 配置 - 确保运行时能找到 Frameworks 目录中的动态库
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# ============================================
# 包含目录
# ============================================

target_include_directories(SuYan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/clipboard
    ${LIBRIME_INCLUDE_DIR}
)

# ============================================
# 链接依赖
# ============================================

target_link_libraries(SuYan PRIVATE
    # 内部库
    suyan_ui
    suyan_core
    suyan_clipboard
    
    # macOS 框架
    ${COCOA_FRAMEWORK}
    ${INPUTMETHODKIT_FRAMEWORK}
    ${CARBON_FRAMEWORK}
    
    # Qt 库
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ============================================
# 资源文件复制到 Bundle
# ============================================

# 复制图标到 Resources
set_source_files_properties(${MACOS_ICON_FILE} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

# 复制 RIME 数据到 SharedSupport/rime
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/SharedSupport/rime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RIME_DATA_DIR}"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/SharedSupport/rime"
    COMMENT "Copying RIME data to bundle..."
)

# 复制主题到 Resources/themes
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Resources/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${THEME_DIR}"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Resources/themes"
    COMMENT "Copying themes to bundle..."
)

# 复制卸载脚本到 Resources
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/scripts/uninstall.sh"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Resources/uninstall.sh"
    COMMAND chmod +x "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Resources/uninstall.sh"
    COMMENT "Copying uninstall script to bundle..."
)

# ============================================
# librime 打包到 Frameworks
# ============================================

# 复制 librime 到 Frameworks 目录
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks"
    # 复制实际的 dylib 文件
    COMMAND ${CMAKE_COMMAND} -E copy
        "${LIBRIME_LIB_DIR}/librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks/"
    # 创建符号链接
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks/librime.dylib"
    COMMENT "Copying librime to Frameworks..."
)

# 修复 librime 的 install_name
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND install_name_tool -id
        "@rpath/librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks/librime.1.dylib"
    COMMENT "Fixing librime install_name..."
)

# 修复可执行文件中对 librime 的引用
add_custom_command(TARGET SuYan POST_BUILD
    COMMAND install_name_tool -change
        "${LIBRIME_LIB_DIR}/librime.dylib"
        "@rpath/librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/MacOS/SuYan"
    COMMAND install_name_tool -change
        "${LIBRIME_LIB_DIR}/librime.1.dylib"
        "@rpath/librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/MacOS/SuYan"
    COMMAND install_name_tool -change
        "@rpath/librime.dylib"
        "@rpath/librime.1.dylib"
        "$<TARGET_BUNDLE_DIR:SuYan>/Contents/MacOS/SuYan"
    COMMENT "Fixing librime references in executable..."
)

# ============================================
# Qt 框架部署
# ============================================

# 查找 macdeployqt 工具
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
    HINTS "${Qt6_DIR}/../../../bin"
          "/opt/homebrew/opt/qt/bin"
          "/usr/local/opt/qt/bin"
)

if(MACDEPLOYQT_EXECUTABLE)
    message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
else()
    message(WARNING "macdeployqt not found, Qt frameworks will not be bundled automatically")
endif()

# 创建修复 Qt 框架引用的脚本
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/fix_qt_refs.sh.in"
    "${CMAKE_BINARY_DIR}/fix_qt_refs.sh"
    @ONLY
)
set(FIX_QT_REFS_SCRIPT "${CMAKE_BINARY_DIR}/fix_qt_refs.sh")

# 添加 Qt 部署目标
# 使用 macdeployqt 自动打包 Qt 框架和插件
if(MACDEPLOYQT_EXECUTABLE)
    add_custom_target(deploy_qt
        COMMAND ${MACDEPLOYQT_EXECUTABLE}
            "$<TARGET_BUNDLE_DIR:SuYan>"
            -always-overwrite
            -verbose=1
        # 运行修复脚本
        COMMAND chmod +x ${FIX_QT_REFS_SCRIPT}
        COMMAND ${FIX_QT_REFS_SCRIPT} "$<TARGET_BUNDLE_DIR:SuYan>"
        DEPENDS SuYan
        COMMENT "Deploying Qt frameworks to bundle and fixing references..."
    )
    
    # 代码签名目标
    # 使用 ad-hoc 签名（-）或开发者证书
    # 对于本地开发测试，使用 ad-hoc 签名即可
    add_custom_target(codesign
        # 首先签名所有 Frameworks 中的动态库和框架
        COMMAND find "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks" -type f -name "*.dylib" -exec codesign --force --sign - --timestamp=none {} +
        COMMAND find "$<TARGET_BUNDLE_DIR:SuYan>/Contents/Frameworks" -type d -name "*.framework" -exec codesign --force --sign - --timestamp=none --deep {} +
        # 签名 PlugIns 目录中的所有内容
        COMMAND codesign --force --sign - --timestamp=none --deep "$<TARGET_BUNDLE_DIR:SuYan>/Contents/PlugIns" || true
        # 最后签名整个 bundle
        COMMAND codesign --force --sign - --timestamp=none --deep "$<TARGET_BUNDLE_DIR:SuYan>"
        DEPENDS deploy_qt
        COMMENT "Code signing the bundle..."
    )
    
    # 创建完整打包目标（包含 Qt 部署和签名）
    add_custom_target(bundle_complete
        DEPENDS codesign
        COMMENT "Creating complete bundle with all dependencies..."
    )
else()
    # 如果没有 macdeployqt，创建一个警告目标
    add_custom_target(deploy_qt
        COMMAND ${CMAKE_COMMAND} -E echo "WARNING: macdeployqt not found. Qt frameworks not bundled."
        DEPENDS SuYan
    )
    
    add_custom_target(codesign
        COMMAND codesign --force --sign - --timestamp=none --deep "$<TARGET_BUNDLE_DIR:SuYan>"
        DEPENDS SuYan
        COMMENT "Code signing the bundle (without Qt frameworks)..."
    )
    
    add_custom_target(bundle_complete
        DEPENDS codesign
        COMMAND ${CMAKE_COMMAND} -E echo "WARNING: Bundle created without Qt frameworks bundled."
    )
endif()

# ============================================
# 安装规则
# ============================================

# 安装 bundle 到 Input Methods 目录
# 使用相对路径，这样可以通过 DESTDIR 或 --prefix 控制安装位置
# 默认安装到 /Library/Input Methods/
# 使用 cmake --install . --prefix / 安装到系统目录
# 使用 cmake --install . --prefix /tmp/test 安装到测试目录
install(TARGETS SuYan
    BUNDLE DESTINATION "Library/Input Methods"
    COMPONENT Runtime
)

# 安装后复制 Qt 框架（如果使用 macdeployqt）
# 注意：macdeployqt 应该在 install 之前运行，所以我们使用 bundle_complete 目标

# 创建安装脚本目录
set(INSTALL_SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/scripts")

# 安装后脚本 - 显示安装说明
install(CODE "
    message(STATUS \"\")
    message(STATUS \"========================================\")
    message(STATUS \"SuYan Input Method installed successfully!\")
    message(STATUS \"========================================\")
    message(STATUS \"\")
    message(STATUS \"To enable the input method:\")
    message(STATUS \"  1. Open System Settings > Keyboard > Input Sources\")
    message(STATUS \"  2. Click '+' and select 'Chinese' > 'SuYan'\")
    message(STATUS \"\")
" COMPONENT Runtime)

# ============================================
# CPack 配置（用于生成安装包）
# ============================================

set(CPACK_GENERATOR "productbuild")
set(CPACK_PACKAGE_NAME "SuYan")
set(CPACK_PACKAGE_VENDOR "zhangjh")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "素言中文输入法")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SuYan")

# PKG 安装包配置
set(CPACK_PRODUCTBUILD_IDENTIFIER "cn.zhangjh.inputmethod.SuYan")

# 可选的 LICENSE 和 README 文件
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
endif()

# 包含 CPack
include(CPack)

# ============================================
# 状态输出
# ============================================

message(STATUS "")
message(STATUS "=== macOS Platform Layer Configuration ===")
message(STATUS "  Info.plist: ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
message(STATUS "  Icon: ${MACOS_ICON_FILE}")
message(STATUS "  RIME data: ${RIME_DATA_DIR}")
message(STATUS "  Themes: ${THEME_DIR}")
message(STATUS "  librime: ${LIBRIME_DYLIB}")
message(STATUS "  macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
message(STATUS "")
message(STATUS "Build targets:")
message(STATUS "  SuYan          - Build the input method bundle")
message(STATUS "  deploy_qt      - Deploy Qt frameworks to bundle")
message(STATUS "  bundle_complete - Create complete bundle with all dependencies")
message(STATUS "")
message(STATUS "Install:")
message(STATUS "  cmake --install . --prefix /")
message(STATUS "  (Installs to /Library/Input Methods/SuYan.app)")
message(STATUS "")
