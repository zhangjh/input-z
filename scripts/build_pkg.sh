#!/bin/bash
# ============================================
# SuYan Input Method - PKG 安装包构建脚本
# ============================================
#
# 用法:
#   ./scripts/build_pkg.sh [选项]
#
# 选项:
#   --skip-build    跳过构建步骤，直接打包
#   --skip-qt       跳过 Qt 框架部署
#   --output DIR    指定输出目录（默认: output/）
#   --version VER   指定版本号（默认: 从 CMakeLists.txt 读取）
#   --help          显示帮助信息
#
# 前置条件:
#   - CMake 3.20+
#   - Qt 6.x (通过 Homebrew 安装)
#   - Xcode Command Line Tools
#
# 输出:
#   - output/SuYan-<version>.pkg  - PKG 安装包
#   - output/SuYan.app            - 完整的应用 bundle（可选）

set -e

# ============================================
# 颜色定义
# ============================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ============================================
# 路径配置
# ============================================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
OUTPUT_DIR="${PROJECT_ROOT}/output"
PKG_SCRIPTS_DIR="${PROJECT_ROOT}/scripts/pkg"

# ============================================
# 默认参数
# ============================================
SKIP_BUILD=false
SKIP_QT=false
VERSION=""

# ============================================
# 帮助信息
# ============================================
show_help() {
    echo "SuYan Input Method - PKG 安装包构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --skip-build    跳过构建步骤，直接打包"
    echo "  --skip-qt       跳过 Qt 框架部署"
    echo "  --output DIR    指定输出目录（默认: output/）"
    echo "  --version VER   指定版本号（默认: 从 CMakeLists.txt 读取）"
    echo "  --help          显示帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                      # 完整构建并打包"
    echo "  $0 --skip-build         # 跳过构建，直接打包"
    echo "  $0 --version 1.0.1      # 指定版本号"
    exit 0
}

# ============================================
# 参数解析
# ============================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-qt)
            SKIP_QT=true
            shift
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --help)
            show_help
            ;;
        *)
            echo -e "${RED}错误: 未知选项 $1${NC}"
            show_help
            ;;
    esac
done

# ============================================
# 获取版本号
# ============================================
get_version() {
    if [ -n "${VERSION}" ]; then
        echo "${VERSION}"
        return
    fi
    
    # 从 CMakeLists.txt 读取版本号
    local version=$(grep -E "^\s*VERSION\s+" "${PROJECT_ROOT}/CMakeLists.txt" | head -1 | sed 's/.*VERSION\s*\([0-9.]*\).*/\1/')
    if [ -z "${version}" ]; then
        version="1.0.0"
    fi
    echo "${version}"
}

# ============================================
# 日志函数
# ============================================
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# ============================================
# 检查依赖
# ============================================
check_dependencies() {
    log_info "检查构建依赖..."
    
    local missing=()
    
    # 检查 CMake
    if ! command -v cmake &> /dev/null; then
        missing+=("cmake")
    fi
    
    # 检查 pkgbuild
    if ! command -v pkgbuild &> /dev/null; then
        missing+=("pkgbuild (Xcode Command Line Tools)")
    fi
    
    # 检查 productbuild
    if ! command -v productbuild &> /dev/null; then
        missing+=("productbuild (Xcode Command Line Tools)")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "缺少以下依赖:"
        for dep in "${missing[@]}"; do
            echo "  - ${dep}"
        done
        echo ""
        echo "请安装 Xcode Command Line Tools:"
        echo "  xcode-select --install"
        exit 1
    fi
    
    log_success "依赖检查通过"
}


# ============================================
# 构建项目
# ============================================
build_project() {
    if [ "${SKIP_BUILD}" = true ]; then
        log_info "跳过构建步骤"
        return
    fi
    
    log_info "开始构建项目..."
    
    # 创建构建目录
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # 配置 CMake
    log_info "配置 CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    
    # 构建
    log_info "编译项目..."
    cmake --build . --target SuYan -j$(sysctl -n hw.ncpu)
    
    # 部署 Qt 框架
    if [ "${SKIP_QT}" = false ]; then
        log_info "部署 Qt 框架..."
        cmake --build . --target deploy_qt
    fi
    
    cd "${PROJECT_ROOT}"
    log_success "项目构建完成"
}

# ============================================
# 验证 Bundle
# ============================================
verify_bundle() {
    local bundle_path="${BUILD_DIR}/bin/SuYan.app"
    
    log_info "验证 Bundle 完整性..."
    
    # 检查 bundle 是否存在
    if [ ! -d "${bundle_path}" ]; then
        log_error "Bundle 不存在: ${bundle_path}"
        exit 1
    fi
    
    # 检查可执行文件
    if [ ! -f "${bundle_path}/Contents/MacOS/SuYan" ]; then
        log_error "可执行文件不存在"
        exit 1
    fi
    
    # 检查 Info.plist
    if [ ! -f "${bundle_path}/Contents/Info.plist" ]; then
        log_error "Info.plist 不存在"
        exit 1
    fi
    
    # 检查 RIME 数据
    if [ ! -d "${bundle_path}/Contents/SharedSupport/rime" ]; then
        log_error "RIME 数据目录不存在"
        exit 1
    fi
    
    # 检查 librime
    if [ ! -f "${bundle_path}/Contents/Frameworks/librime.1.dylib" ]; then
        log_error "librime 库不存在"
        exit 1
    fi
    
    # 检查 Qt 框架（如果没有跳过）
    if [ "${SKIP_QT}" = false ]; then
        if [ ! -d "${bundle_path}/Contents/Frameworks/QtCore.framework" ]; then
            log_warning "Qt 框架未部署，可能需要手动运行 macdeployqt"
        fi
    fi
    
    log_success "Bundle 验证通过"
}

# ============================================
# 创建 PKG 安装包
# ============================================
create_pkg() {
    local version=$(get_version)
    local bundle_path="${BUILD_DIR}/bin/SuYan.app"
    local pkg_name="SuYan-${version}.pkg"
    local component_pkg="${OUTPUT_DIR}/SuYan-component.pkg"
    local final_pkg="${OUTPUT_DIR}/${pkg_name}"
    
    log_info "创建 PKG 安装包 (版本: ${version})..."
    
    # 创建输出目录
    mkdir -p "${OUTPUT_DIR}"
    
    # 创建临时安装根目录
    local install_root="${OUTPUT_DIR}/install_root"
    rm -rf "${install_root}"
    mkdir -p "${install_root}/Library/Input Methods"
    
    # 复制 bundle 到安装根目录
    log_info "复制 Bundle 到安装根目录..."
    cp -R "${bundle_path}" "${install_root}/Library/Input Methods/"
    
    # 对应用进行 ad-hoc 签名（macOS 安全策略要求）
    log_info "对应用进行 ad-hoc 签名..."
    local installed_bundle="${install_root}/Library/Input Methods/SuYan.app"
    
    # 使用 --force --deep --sign - 进行 ad-hoc 签名
    codesign --force --deep --sign - "${installed_bundle}" 2>/dev/null || {
        log_warning "ad-hoc 签名失败，尝试移除签名..."
        # 如果 ad-hoc 签名失败，尝试移除签名
        find "${installed_bundle}/Contents/Frameworks" -name "*.framework" -exec codesign --remove-signature {} \; 2>/dev/null || true
        find "${installed_bundle}/Contents/Frameworks" -name "*.dylib" -exec codesign --remove-signature {} \; 2>/dev/null || true
        codesign --remove-signature "${installed_bundle}" 2>/dev/null || true
    }
    
    # 创建组件包
    log_info "创建组件包..."
    pkgbuild \
        --root "${install_root}" \
        --identifier "cn.zhangjh.inputmethod.SuYan" \
        --version "${version}" \
        --install-location "/" \
        --scripts "${PKG_SCRIPTS_DIR}" \
        "${component_pkg}"
    
    # 创建分发描述文件
    local distribution_xml="${OUTPUT_DIR}/distribution.xml"
    create_distribution_xml "${distribution_xml}" "${version}"
    
    # 创建最终安装包
    log_info "创建最终安装包..."
    productbuild \
        --distribution "${distribution_xml}" \
        --package-path "${OUTPUT_DIR}" \
        --resources "${PKG_SCRIPTS_DIR}/resources" \
        "${final_pkg}"
    
    # 清理临时文件
    log_info "清理临时文件..."
    rm -rf "${install_root}"
    rm -f "${component_pkg}"
    rm -f "${distribution_xml}"
    
    log_success "PKG 安装包创建完成: ${final_pkg}"
    
    # 显示包信息
    echo ""
    echo "============================================"
    echo "  安装包信息"
    echo "============================================"
    echo "  文件: ${final_pkg}"
    echo "  大小: $(du -h "${final_pkg}" | cut -f1)"
    echo "  版本: ${version}"
    echo ""
    echo "安装方法:"
    echo "  双击 ${pkg_name} 运行安装程序"
    echo "  或使用命令行: sudo installer -pkg ${final_pkg} -target /"
    echo ""
}

# ============================================
# 创建分发描述文件
# ============================================
create_distribution_xml() {
    local output_file="$1"
    local version="$2"
    
    cat > "${output_file}" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>素言输入法</title>
    <organization>cn.zhangjh</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="true" rootVolumeOnly="true"/>
    
    <!-- 欢迎页面 -->
    <welcome file="welcome.html"/>
    
    <!-- 许可协议（可选） -->
    <!-- <license file="license.html"/> -->
    
    <!-- 结束页面 -->
    <conclusion file="conclusion.html"/>
    
    <!-- 系统要求 -->
    <os-version min="13.0"/>
    
    <!-- 安装选项 -->
    <choices-outline>
        <line choice="default">
            <line choice="cn.zhangjh.inputmethod.SuYan"/>
        </line>
    </choices-outline>
    
    <choice id="default"/>
    
    <choice id="cn.zhangjh.inputmethod.SuYan" visible="false">
        <pkg-ref id="cn.zhangjh.inputmethod.SuYan"/>
    </choice>
    
    <pkg-ref id="cn.zhangjh.inputmethod.SuYan" version="${version}" onConclusion="none">SuYan-component.pkg</pkg-ref>
</installer-gui-script>
EOF
}

# ============================================
# 主函数
# ============================================
main() {
    echo ""
    echo "============================================"
    echo "  SuYan Input Method - PKG 构建脚本"
    echo "============================================"
    echo ""
    
    # 检查依赖
    check_dependencies
    
    # 确保 PKG 脚本目录存在
    if [ ! -d "${PKG_SCRIPTS_DIR}" ]; then
        log_error "PKG 脚本目录不存在: ${PKG_SCRIPTS_DIR}"
        log_info "请先运行 scripts/setup_pkg_scripts.sh 创建必要的脚本文件"
        exit 1
    fi
    
    # 构建项目
    build_project
    
    # 验证 Bundle
    verify_bundle
    
    # 创建 PKG
    create_pkg
    
    echo ""
    log_success "所有步骤完成！"
    echo ""
}

# 运行主函数
main
