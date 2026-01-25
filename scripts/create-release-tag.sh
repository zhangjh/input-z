#!/bin/bash
# ============================================
# SuYan - 创建发布 Tag 脚本
# ============================================
#
# 用法:
#   ./scripts/create-release-tag.sh [选项]
#
# 选项:
#   (无参数)    使用 brand.conf 中的 IME_VERSION 创建 tag
#   patch       递增补丁版本 (1.0.0 -> 1.0.1)
#   minor       递增次版本号 (1.0.0 -> 1.1.0)
#   major       递增主版本号 (1.0.0 -> 2.0.0)
#   1.2.3       直接指定版本号
#
# 版本配置文件: brand.conf (IME_VERSION)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BRAND_CONF="${PROJECT_ROOT}/brand.conf"

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 从 brand.conf 读取版本
get_brand_version() {
    if [ -f "${BRAND_CONF}" ]; then
        local version=$(grep -E "^IME_VERSION=" "${BRAND_CONF}" | cut -d'"' -f2)
        if [ -n "${version}" ]; then
            echo "${version}"
            return
        fi
    fi
    echo "1.0.0"
}

# 获取最新 tag 版本
get_latest_tag_version() {
    local latest_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
    echo "${latest_tag#v}"
}

# 递增版本号
increment_version() {
    local version="$1"
    local type="$2"
    
    IFS='.' read -r major minor patch <<< "${version}"
    
    case "${type}" in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
    esac
    
    echo "${major}.${minor}.${patch}"
}

# 主函数
main() {
    cd "${PROJECT_ROOT}"
    
    # 检查是否在 git 仓库中
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        log_error "当前目录不是 git 仓库"
        exit 1
    fi
    
    # 获取当前版本
    local brand_version=$(get_brand_version)
    local tag_version=$(get_latest_tag_version)
    
    log_info "brand.conf 版本: ${brand_version}"
    log_info "最新 tag 版本: ${tag_version}"
    
    # 确定新版本号
    local new_version=""
    local input="$1"
    
    if [ -z "${input}" ]; then
        new_version="${brand_version}"
    elif [[ "${input}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        new_version="${input}"
    elif [ "${input}" = "major" ] || [ "${input}" = "minor" ] || [ "${input}" = "patch" ]; then
        new_version=$(increment_version "${brand_version}" "${input}")
    else
        log_error "无效的参数: ${input}"
        echo "用法: $0 [版本号|major|minor|patch]"
        exit 1
    fi
    
    # 检查 tag 是否已存在
    if git rev-parse "v${new_version}" >/dev/null 2>&1; then
        log_error "Tag v${new_version} 已存在"
        exit 1
    fi
    
    echo ""
    log_info "将创建版本: ${new_version}"
    echo ""
    
    # 确认
    read -p "确认创建 tag v${new_version}？(y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_error "已取消"
        exit 1
    fi
    
    # 创建 tag
    git tag -a "v${new_version}" -m "Release v${new_version}"
    log_success "已创建 tag: v${new_version}"
    
    # 推送 tag
    echo ""
    read -p "是否推送 tag 到远程仓库？(Y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        git push origin "v${new_version}"
        log_success "已推送 tag"
        echo ""
        log_info "GitHub Actions 将自动开始构建..."
        log_info "构建完成后，请到 GitHub Releases 页面发布草稿"
    else
        log_warning "未推送"
        echo "手动推送: git push origin v${new_version}"
    fi
    
    echo ""
    log_success "完成！"
}

main "$@"
