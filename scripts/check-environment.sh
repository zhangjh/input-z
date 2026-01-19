#!/bin/bash
# 检查构建环境
# 用法: ./scripts/check-environment.sh

set -e

echo "=== CrossPlatformIME 构建环境检查 ==="
echo ""

# 检测操作系统
OS="$(uname -s)"
echo "操作系统: $OS"

check_tool() {
    local tool=$1
    local install_hint=$2
    if command -v $tool &> /dev/null; then
        local version=$($tool --version 2>&1 | head -n 1)
        echo "✓ $tool: $version"
        return 0
    else
        echo "✗ $tool: 未安装"
        echo "  安装方法: $install_hint"
        return 1
    fi
}

echo ""
echo "=== 必需工具 ==="

MISSING=0

# 根据操作系统选择安装提示
case "$OS" in
    Darwin)
        GIT_HINT="brew install git"
        CMAKE_HINT="brew install cmake"
        NINJA_HINT="brew install ninja"
        CCACHE_HINT="brew install ccache"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        GIT_HINT="winget install Git.Git"
        CMAKE_HINT="winget install Kitware.CMake"
        NINJA_HINT="winget install Ninja-build.Ninja"
        CCACHE_HINT="winget install ccache.ccache"
        ;;
    *)
        GIT_HINT="apt install git"
        CMAKE_HINT="apt install cmake"
        NINJA_HINT="apt install ninja-build"
        CCACHE_HINT="apt install ccache"
        ;;
esac

check_tool "git" "$GIT_HINT" || MISSING=1
check_tool "cmake" "$CMAKE_HINT" || MISSING=1

if [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
    echo ""
    echo "=== Windows 特定检查 ==="
    
    # 检查 Visual Studio 或 MSVC
    if command -v cl &> /dev/null; then
        echo "✓ MSVC (cl.exe): 已找到"
    else
        echo "○ MSVC (cl.exe): 未在 PATH 中找到"
        echo "  提示: 请从 Visual Studio Developer Command Prompt 运行，或安装 Visual Studio Build Tools"
        echo "  安装方法: winget install Microsoft.VisualStudio.2022.BuildTools"
    fi
    
    # 检查 Windows SDK
    if [ -d "/c/Program Files (x86)/Windows Kits/10" ] || [ -d "/c/Program Files/Windows Kits/10" ]; then
        echo "✓ Windows SDK: 已安装"
    else
        echo "○ Windows SDK: 未找到"
        echo "  提示: 通过 Visual Studio Installer 安装 Windows 10/11 SDK"
    fi

elif [ "$OS" == "Darwin" ]; then
    echo ""
    echo "=== macOS 特定检查 ==="
    
    # 检查 Xcode 命令行工具
    if xcode-select -p &> /dev/null; then
        echo "✓ Xcode 命令行工具: $(xcode-select -p)"
    else
        echo "✗ Xcode 命令行工具: 未安装"
        echo "  安装方法: xcode-select --install"
        MISSING=1
    fi
    
    # 检查编译器
    if command -v clang++ &> /dev/null; then
        echo "✓ clang++: $(clang++ --version | head -n 1)"
    else
        echo "✗ clang++: 未安装"
        MISSING=1
    fi
fi

echo ""
echo "=== 可选工具 ==="

check_tool "ninja" "$NINJA_HINT" || true
check_tool "ccache" "$CCACHE_HINT" || true

echo ""
echo "=== Git Submodules 状态 ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

check_submodule() {
    local path=$1
    local name=$2
    if [ -f "$PROJECT_ROOT/$path/CMakeLists.txt" ] || [ -f "$PROJECT_ROOT/$path/Makefile" ]; then
        echo "✓ $name: 已初始化"
    else
        echo "○ $name: 未初始化 (运行 git submodule update --init --recursive)"
    fi
}

check_submodule "deps/librime" "librime"
check_submodule "deps/weasel" "weasel"
check_submodule "deps/squirrel" "squirrel"
check_submodule "deps/rime-ice" "rime-ice"

echo ""
if [ $MISSING -eq 0 ]; then
    echo "=== 环境检查通过 ==="
    echo "可以开始构建项目"
else
    echo "=== 环境检查失败 ==="
    echo "请安装缺失的工具后重试"
    exit 1
fi
