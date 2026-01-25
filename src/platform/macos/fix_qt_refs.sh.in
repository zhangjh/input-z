#!/bin/bash
# 修复 Qt 框架引用脚本
# 将所有对 Homebrew 的引用改为 bundle 内的路径
# 这个脚本会彻底修复所有二进制文件，避免 Qt 库冲突

set -e

BUNDLE_PATH="$1"
EXECUTABLE="${BUNDLE_PATH}/Contents/MacOS/SuYan"
FRAMEWORKS_DIR="${BUNDLE_PATH}/Contents/Frameworks"
PLUGINS_DIR="${BUNDLE_PATH}/Contents/PlugIns"

echo "=========================================="
echo "Fixing all library references in bundle..."
echo "=========================================="

# 通用函数：修复单个二进制文件中的所有 Homebrew 引用
fix_binary_refs() {
    local BINARY="$1"
    local BINARY_NAME=$(basename "$BINARY")
    
    if [ ! -f "$BINARY" ]; then
        return
    fi
    
    # 获取所有 Homebrew 引用
    local REFS=$(otool -L "$BINARY" 2>/dev/null | grep -E '/opt/homebrew|/usr/local' | awk '{print $1}' || true)
    
    for REF in ${REFS}; do
        local LIB_NAME=$(basename "$REF")
        local NEW_PATH=""
        
        # 判断是 Qt 框架还是普通动态库
        if [[ "$REF" == *".framework"* ]]; then
            # Qt 框架
            local FRAMEWORK_PATH=$(echo "$REF" | sed 's|.*/\(Qt[^/]*\.framework/.*\)|\1|')
            NEW_PATH="@executable_path/../Frameworks/${FRAMEWORK_PATH}"
        else
            # 普通动态库
            NEW_PATH="@executable_path/../Frameworks/${LIB_NAME}"
        fi
        
        echo "  [${BINARY_NAME}] ${REF} -> ${NEW_PATH}"
        install_name_tool -change "$REF" "$NEW_PATH" "$BINARY" 2>/dev/null || true
    done
}

# 通用函数：修复二进制文件的 install_name (ID)
fix_binary_id() {
    local BINARY="$1"
    local NEW_ID="$2"
    
    if [ ! -f "$BINARY" ]; then
        return
    fi
    
    install_name_tool -id "$NEW_ID" "$BINARY" 2>/dev/null || true
}

# 1. 修复主可执行文件
echo ""
echo "1. Fixing main executable..."
fix_binary_refs "$EXECUTABLE"

# 2. 修复 Frameworks 目录中的所有 Qt 框架
echo ""
echo "2. Fixing Qt frameworks..."
for FRAMEWORK in "${FRAMEWORKS_DIR}"/Qt*.framework; do
    if [ -d "${FRAMEWORK}" ]; then
        FRAMEWORK_NAME=$(basename "${FRAMEWORK}" .framework)
        FRAMEWORK_BINARY="${FRAMEWORK}/Versions/A/${FRAMEWORK_NAME}"
        
        if [ -f "${FRAMEWORK_BINARY}" ]; then
            echo "  Processing ${FRAMEWORK_NAME}..."
            # 修复框架 ID
            fix_binary_id "${FRAMEWORK_BINARY}" "@rpath/${FRAMEWORK_NAME}.framework/Versions/A/${FRAMEWORK_NAME}"
            # 修复框架内部引用
            fix_binary_refs "${FRAMEWORK_BINARY}"
        fi
    fi
done

# 3. 修复 Frameworks 目录中的所有动态库
echo ""
echo "3. Fixing dynamic libraries in Frameworks..."
for DYLIB in "${FRAMEWORKS_DIR}"/*.dylib; do
    if [ -f "${DYLIB}" ]; then
        DYLIB_NAME=$(basename "${DYLIB}")
        echo "  Processing ${DYLIB_NAME}..."
        # 修复动态库 ID
        fix_binary_id "${DYLIB}" "@executable_path/../Frameworks/${DYLIB_NAME}"
        # 修复动态库内部引用
        fix_binary_refs "${DYLIB}"
    fi
done

# 4. 修复所有插件
echo ""
echo "4. Fixing plugins..."
if [ -d "${PLUGINS_DIR}" ]; then
    find "${PLUGINS_DIR}" -name "*.dylib" -type f | while read PLUGIN; do
        PLUGIN_NAME=$(basename "${PLUGIN}")
        echo "  Processing plugin: ${PLUGIN_NAME}..."
        fix_binary_refs "${PLUGIN}"
    done
fi

# 5. 验证：检查是否还有 Homebrew 引用残留
echo ""
echo "5. Verifying no Homebrew references remain..."
REMAINING=$(find "${BUNDLE_PATH}/Contents" \( -name "*.dylib" -o -name "*.framework" \) -type f -exec otool -L {} \; 2>/dev/null | grep -E '/opt/homebrew|/usr/local' || true)

if [ -n "$REMAINING" ]; then
    echo "WARNING: Some Homebrew references still remain:"
    echo "$REMAINING"
else
    echo "  All references fixed successfully!"
fi

echo ""
echo "=========================================="
echo "Done fixing references."
echo "=========================================="
