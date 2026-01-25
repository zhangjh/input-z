#!/bin/bash
# 修复 Qt 框架引用脚本
# 将可执行文件中对 Homebrew Qt 的引用改为 @rpath

BUNDLE_PATH="$1"
EXECUTABLE="${BUNDLE_PATH}/Contents/MacOS/SuYan"
FRAMEWORKS_DIR="${BUNDLE_PATH}/Contents/Frameworks"

echo "Fixing Qt framework references in ${EXECUTABLE}..."

# 获取所有 Qt 框架引用
QT_REFS=$(otool -L "${EXECUTABLE}" | grep -E '/opt/homebrew.*Qt.*\.framework' | awk '{print $1}')

for REF in ${QT_REFS}; do
    # 提取框架名称（如 QtCore.framework/Versions/A/QtCore）
    FRAMEWORK_PATH=$(echo "${REF}" | sed 's|.*/\(Qt[^/]*\.framework/.*\)|\1|')
    FRAMEWORK_NAME=$(echo "${FRAMEWORK_PATH}" | sed 's|\(Qt[^/]*\)\.framework/.*|\1|')
    
    echo "  Fixing ${FRAMEWORK_NAME}: ${REF} -> @rpath/${FRAMEWORK_PATH}"
    install_name_tool -change "${REF}" "@rpath/${FRAMEWORK_PATH}" "${EXECUTABLE}"
done

# 同时修复 Frameworks 目录中的 Qt 框架内部引用
echo "Fixing Qt framework internal references..."
for FRAMEWORK in "${FRAMEWORKS_DIR}"/Qt*.framework; do
    if [ -d "${FRAMEWORK}" ]; then
        FRAMEWORK_NAME=$(basename "${FRAMEWORK}" .framework)
        FRAMEWORK_BINARY="${FRAMEWORK}/Versions/A/${FRAMEWORK_NAME}"
        
        if [ -f "${FRAMEWORK_BINARY}" ]; then
            # 修复框架自身的 ID
            install_name_tool -id "@rpath/${FRAMEWORK_NAME}.framework/Versions/A/${FRAMEWORK_NAME}" "${FRAMEWORK_BINARY}" 2>/dev/null || true
            
            # 修复框架对其他 Qt 框架的引用
            INTERNAL_REFS=$(otool -L "${FRAMEWORK_BINARY}" | grep -E '/opt/homebrew.*Qt.*\.framework' | awk '{print $1}')
            for IREF in ${INTERNAL_REFS}; do
                IFRAMEWORK_PATH=$(echo "${IREF}" | sed 's|.*/\(Qt[^/]*\.framework/.*\)|\1|')
                install_name_tool -change "${IREF}" "@rpath/${IFRAMEWORK_PATH}" "${FRAMEWORK_BINARY}" 2>/dev/null || true
            done
        fi
    fi
done

# 修复其他动态库的引用（如 libyaml-cpp）
echo "Fixing other library references..."
OTHER_REFS=$(otool -L "${EXECUTABLE}" | grep -E '/opt/homebrew' | grep -v Qt | awk '{print $1}')
for REF in ${OTHER_REFS}; do
    LIB_NAME=$(basename "${REF}")
    if [ -f "${FRAMEWORKS_DIR}/${LIB_NAME}" ]; then
        echo "  Fixing ${LIB_NAME}: ${REF} -> @rpath/${LIB_NAME}"
        install_name_tool -change "${REF}" "@rpath/${LIB_NAME}" "${EXECUTABLE}"
    fi
done

echo "Done fixing references."
