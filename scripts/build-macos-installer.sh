#!/bin/bash
# macOS Full Build Script
#
# This script builds the complete macOS installer including:
#   1. Download prebuilt librime (with plugins)
#   2. Build Squirrel frontend
#   3. Build PKG installer package
#
# Usage:
#   ./build-macos-installer.sh [options]
#
# Options:
#   all       - Build everything (default)
#   deps      - Download dependencies only
#   app       - Build app only
#   package   - Build package only
#   clean     - Clean build artifacts
#   sign      - Sign and notarize (requires DEV_ID env var)
#
# Environment Variables:
#   IME_VERSION           - Version number (default: 1.0.0)
#   IME_BRAND_NAME        - App display name (default: SuYan)
#   IME_BRAND_IDENTIFIER  - Bundle identifier (default: cn.zhangjh.inputmethod.SuYan)
#                           NOTE: Must contain "inputmethod" for macOS recognition
#   DEV_ID                - Apple Developer ID for signing
#
# Examples:
#   ./build-macos-installer.sh all
#   IME_BRAND_NAME=智拼 IME_BRAND_IDENTIFIER=cn.zhangjh.zhipin ./build-macos-installer.sh all
#
# Prerequisites:
#   - Xcode Command Line Tools
#   - curl

set -e

# Get script directory first (needed for config file path)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ============================================
# Load brand configuration
# ============================================
BRAND_CONF="${PROJECT_ROOT}/brand.conf"
if [ -f "${BRAND_CONF}" ]; then
    echo "Loading brand configuration from: ${BRAND_CONF}"
    # Source the config file (only export non-comment, non-empty lines)
    set -a  # Auto-export all variables
    source "${BRAND_CONF}"
    set +a
fi

# Apply defaults for required variables (if not set in brand.conf or environment)
export IME_BRAND_NAME="${IME_BRAND_NAME:-SuYan}"
# Note: The identifier MUST contain "inputmethod" for macOS to recognize it as an input method
# Format: domain.company.inputmethod.name (e.g., cn.zhangjh.inputmethod.SuYan)
export IME_BRAND_IDENTIFIER="${IME_BRAND_IDENTIFIER:-cn.zhangjh.inputmethod.SuYan}"
export IME_REQUIRE_LOGOUT="${IME_REQUIRE_LOGOUT:-false}"
export IME_AUTO_OPEN_SETTINGS="${IME_AUTO_OPEN_SETTINGS:-true}"
export IME_PREBUILD_RIME="${IME_PREBUILD_RIME:-true}"

echo "============================================"
echo "${IME_BRAND_NAME} macOS Full Build"
echo "============================================"
echo ""

# Set default version
IME_VERSION="${IME_VERSION:-1.0.0}"

# librime prebuilt version (from official releases)
RIME_VERSION="1.16.0"
RIME_GIT_HASH="a251145"
SPARKLE_VERSION="2.6.2"

echo "Project Root: ${PROJECT_ROOT}"
echo "Brand: ${IME_BRAND_NAME} (${IME_BRAND_IDENTIFIER})"
echo "Version: ${IME_VERSION}"
echo "librime Version: ${RIME_VERSION}"
echo ""

# Parse command line
BUILD_DEPS=0
BUILD_APP=0
BUILD_PACKAGE=0
BUILD_CLEAN=0
BUILD_SIGN=0

if [ $# -eq 0 ]; then
    BUILD_DEPS=1
    BUILD_APP=1
    BUILD_PACKAGE=1
fi

for arg in "$@"; do
    case $arg in
        all)
            BUILD_DEPS=1
            BUILD_APP=1
            BUILD_PACKAGE=1
            ;;
        deps)
            BUILD_DEPS=1
            ;;
        app)
            BUILD_APP=1
            ;;
        package)
            BUILD_PACKAGE=1
            ;;
        clean)
            BUILD_CLEAN=1
            ;;
        sign)
            BUILD_SIGN=1
            ;;
    esac
done

# Clean if requested
if [ $BUILD_CLEAN -eq 1 ]; then
    echo "Cleaning build artifacts..."
    rm -rf "${PROJECT_ROOT}/output/macos"
    rm -rf "${PROJECT_ROOT}/deps/squirrel/build"
    rm -rf "${PROJECT_ROOT}/deps/squirrel/lib"
    rm -rf "${PROJECT_ROOT}/deps/squirrel/bin"
    rm -rf "${PROJECT_ROOT}/deps/squirrel/Frameworks"
    rm -rf "${PROJECT_ROOT}/deps/squirrel/download"
    # Remove librime directory (but not if it's a symlink to preserve submodule)
    if [ -d "${PROJECT_ROOT}/deps/squirrel/librime" ] && [ ! -L "${PROJECT_ROOT}/deps/squirrel/librime" ]; then
        rm -rf "${PROJECT_ROOT}/deps/squirrel/librime"
    fi
    echo "Clean completed."
    
    if [ $BUILD_DEPS -eq 0 ] && [ $BUILD_APP -eq 0 ] && [ $BUILD_PACKAGE -eq 0 ]; then
        exit 0
    fi
fi

# Check for Xcode
if ! command -v xcodebuild &> /dev/null; then
    echo "ERROR: Xcode Command Line Tools not found!"
    echo "Please install with: xcode-select --install"
    exit 1
fi

# Create output directory
mkdir -p "${PROJECT_ROOT}/output/macos"

# Download dependencies (prebuilt librime)
if [ $BUILD_DEPS -eq 1 ]; then
    echo ""
    echo "============================================"
    echo "Downloading Dependencies"
    echo "============================================"
    echo ""
    
    cd "${PROJECT_ROOT}/deps/squirrel"
    
    # Define download URLs
    RIME_ARCHIVE="rime-${RIME_GIT_HASH}-macOS-universal.tar.bz2"
    RIME_DOWNLOAD_URL="https://github.com/rime/librime/releases/download/${RIME_VERSION}/${RIME_ARCHIVE}"
    
    RIME_DEPS_ARCHIVE="rime-deps-${RIME_GIT_HASH}-macOS-universal.tar.bz2"
    RIME_DEPS_DOWNLOAD_URL="https://github.com/rime/librime/releases/download/${RIME_VERSION}/${RIME_DEPS_ARCHIVE}"
    
    SPARKLE_ARCHIVE="Sparkle-${SPARKLE_VERSION}.tar.xz"
    SPARKLE_DOWNLOAD_URL="https://github.com/sparkle-project/Sparkle/releases/download/${SPARKLE_VERSION}/${SPARKLE_ARCHIVE}"
    
    # Create download directory
    mkdir -p download
    cd download
    
    # Download librime
    if [ ! -f "${RIME_ARCHIVE}" ]; then
        echo "Downloading librime ${RIME_VERSION}..."
        curl -LO "${RIME_DOWNLOAD_URL}"
    else
        echo "Using cached librime archive."
    fi
    echo "Extracting librime..."
    tar --bzip2 -xf "${RIME_ARCHIVE}"
    
    # Download librime deps (opencc data)
    if [ ! -f "${RIME_DEPS_ARCHIVE}" ]; then
        echo "Downloading librime deps..."
        curl -LO "${RIME_DEPS_DOWNLOAD_URL}"
    else
        echo "Using cached librime deps archive."
    fi
    echo "Extracting librime deps..."
    tar --bzip2 -xf "${RIME_DEPS_ARCHIVE}"
    
    # Download Sparkle
    if [ ! -f "${SPARKLE_ARCHIVE}" ]; then
        echo "Downloading Sparkle ${SPARKLE_VERSION}..."
        curl -LO "${SPARKLE_DOWNLOAD_URL}"
    else
        echo "Using cached Sparkle archive."
    fi
    echo "Extracting Sparkle..."
    tar -xJf "${SPARKLE_ARCHIVE}"
    
    cd ..
    
    # Setup librime directory
    # Squirrel needs librime source headers (src/rime/*.h) for compilation
    # but uses prebuilt library files
    if [ -L "librime" ]; then
        echo "Removing old librime symlink..."
        rm -f librime
    fi
    if [ -d "librime" ] && [ ! -L "librime" ]; then
        echo "Removing old librime directory..."
        rm -rf librime
    fi
    
    # Create librime directory structure
    mkdir -p librime/dist
    mkdir -p librime/share
    mkdir -p librime/include
    mkdir -p librime/src
    
    # Copy prebuilt files to dist
    cp -R download/dist/* librime/dist/
    cp -R download/share/opencc librime/share/
    
    # Copy headers for Xcode
    cp -R download/dist/include/* librime/include/
    
    # Link to project's librime source for internal headers (rime/key_table.h etc)
    # These are needed for compilation but not included in prebuilt package
    if [ -d "${PROJECT_ROOT}/deps/librime/src/rime" ]; then
        cp -R "${PROJECT_ROOT}/deps/librime/src/rime" librime/src/
        echo "Copied librime source headers from deps/librime"
    else
        echo "WARNING: deps/librime/src/rime not found!"
        echo "Please run: git submodule update --init deps/librime"
        exit 1
    fi
    
    # Setup other directories
    mkdir -p Frameworks
    mkdir -p lib
    mkdir -p bin
    
    # Copy Sparkle framework
    cp -R download/Sparkle.framework Frameworks/
    
    # Copy rime binaries and plugins
    echo "Copying rime binaries..."
    cp -L librime/dist/lib/librime.1.dylib lib/
    cp -pR librime/dist/lib/rime-plugins lib/
    cp librime/dist/bin/rime_deployer bin/
    cp librime/dist/bin/rime_dict_manager bin/
    cp plum/rime-install bin/
    
    # Fix install names
    install_name_tool -change @rpath/librime.1.dylib @loader_path/../Frameworks/librime.1.dylib bin/rime_deployer 2>/dev/null || true
    install_name_tool -change @rpath/librime.1.dylib @loader_path/../Frameworks/librime.1.dylib bin/rime_dict_manager 2>/dev/null || true
    
    # Setup plum for input schemas
    echo "Setting up plum..."
    git submodule update --init plum
    
    # Install default Rime recipes
    echo "Installing Rime recipes..."
    rime_dir=plum/output bash plum/rime-install
    
    # Copy plum data
    mkdir -p SharedSupport
    cp -R plum/output/* SharedSupport/ 2>/dev/null || true
    
    # Copy opencc data
    cp -R librime/share/opencc SharedSupport/
    
    # Copy project custom Rime configurations (overrides default)
    if [ -d "${PROJECT_ROOT}/data/rime" ]; then
        echo "Copying custom Rime configurations..."
        cp -R "${PROJECT_ROOT}/data/rime/"* SharedSupport/
        echo "  - Custom schemas and configurations applied"
    fi
    
    # Copy rime-ice dictionaries for ime_pinyin schema
    if [ -d "${PROJECT_ROOT}/deps/rime-ice/cn_dicts" ]; then
        echo "Copying rime-ice Chinese dictionaries..."
        mkdir -p SharedSupport/cn_dicts
        cp -R "${PROJECT_ROOT}/deps/rime-ice/cn_dicts/"* SharedSupport/cn_dicts/
        echo "  - Chinese dictionaries copied"
    fi
    
    if [ -d "${PROJECT_ROOT}/deps/rime-ice/en_dicts" ]; then
        echo "Copying rime-ice English dictionaries..."
        mkdir -p SharedSupport/en_dicts
        cp -R "${PROJECT_ROOT}/deps/rime-ice/en_dicts/"* SharedSupport/en_dicts/
        echo "  - English dictionaries copied"
    fi
    
    echo ""
    echo "Dependencies downloaded and configured successfully."
    echo "  - librime ${RIME_VERSION} (with lua, octagram, predict plugins)"
    echo "  - Sparkle ${SPARKLE_VERSION}"
    echo "  - OpenCC data"
    echo "  - Default Rime schemas"
fi

# Build app
if [ $BUILD_APP -eq 1 ]; then
    echo ""
    echo "============================================"
    echo "Building ${IME_BRAND_NAME} App"
    echo "============================================"
    echo ""
    
    cd "${PROJECT_ROOT}/deps/squirrel"
    
    # Ensure Sparkle framework is available
    if [ ! -d "Frameworks/Sparkle.framework" ]; then
        echo "ERROR: Sparkle framework not found!"
        echo "Please run with 'deps' option first."
        exit 1
    fi
    
    # ============================================
    # Build and integrate IME core modules
    # ============================================
    echo "Building IME core modules..."
    
    IME_BUILD_DIR="${PROJECT_ROOT}/build/macos-Release"
    mkdir -p "${IME_BUILD_DIR}"
    cd "${IME_BUILD_DIR}"
    
    cmake "${PROJECT_ROOT}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DBUILD_MACOS_FRONTEND=OFF \
        -DENABLE_CLOUD_SYNC=OFF \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0"
    
    cmake --build . --parallel $(sysctl -n hw.ncpu)
    
    echo "IME core modules built successfully."
    
    # Copy integration library and headers to Squirrel
    echo "Copying integration files to Squirrel..."
    cd "${PROJECT_ROOT}/deps/squirrel"
    
    # Create directories
    mkdir -p ime_integration/lib
    mkdir -p ime_integration/include
    
    # Copy static libraries
    cp "${IME_BUILD_DIR}/lib/libime_storage.a" ime_integration/lib/
    cp "${IME_BUILD_DIR}/lib/libime_frequency.a" ime_integration/lib/
    cp "${IME_BUILD_DIR}/lib/libime_input.a" ime_integration/lib/
    cp "${IME_BUILD_DIR}/lib/libime_learning.a" ime_integration/lib/
    cp "${IME_BUILD_DIR}/lib/libime_platform_macos.a" ime_integration/lib/
    
    # Copy headers
    cp "${PROJECT_ROOT}/src/platform/macos/squirrel_integration.h" ime_integration/include/
    cp "${PROJECT_ROOT}/src/core/storage/local_storage.h" ime_integration/include/
    cp "${PROJECT_ROOT}/src/core/frequency/frequency_manager.h" ime_integration/include/
    cp "${PROJECT_ROOT}/src/core/input/candidate_merger.h" ime_integration/include/
    cp "${PROJECT_ROOT}/src/core/input/input_state.h" ime_integration/include/
    cp "${PROJECT_ROOT}/src/core/learning/auto_learner.h" ime_integration/include/
    
    echo "Integration files copied."
    
    cd "${PROJECT_ROOT}/deps/squirrel"
    
    # ============================================
    # Apply brand customization to source code
    # ============================================
    echo "Applying brand customization to source code..."
    
    # Backup original files
    cp sources/InputSource.swift sources/InputSource.swift.bak
    cp sources/Main.swift sources/Main.swift.bak
    cp resources/Info.plist resources/Info.plist.bak
    
    # Replace input source IDs in InputSource.swift
    sed -i '' "s|im.rime.inputmethod.Squirrel.Hans|${IME_BRAND_IDENTIFIER}.Hans|g" sources/InputSource.swift
    sed -i '' "s|im.rime.inputmethod.Squirrel.Hant|${IME_BRAND_IDENTIFIER}.Hant|g" sources/InputSource.swift
    sed -i '' "s|Squirrel method|${IME_BRAND_NAME} method|g" sources/InputSource.swift
    
    # Replace app directory path in Main.swift
    sed -i '' "s|/Library/Input Library/Squirrel.app|/Library/Input Methods/${IME_BRAND_NAME}.app|g" sources/Main.swift
    sed -i '' "s|rime.squirrel|rime.${IME_BRAND_NAME}|g" sources/Main.swift
    
    # Replace RIME traits in SquirrelApplicationDelegate.swift (for Rime initialization)
    # This is CRITICAL: distribution_code_name and distribution_name must match
    # to prevent conflicts with other RIME-based IMEs and ensure proper schema loading
    cp sources/SquirrelApplicationDelegate.swift sources/SquirrelApplicationDelegate.swift.bak
    
    # Replace app_name (used for log directory)
    sed -i '' "s|rime.squirrel|rime.${IME_BRAND_NAME}|g" sources/SquirrelApplicationDelegate.swift
    
    # Replace distribution_code_name - this affects installation.yaml and schema loading
    sed -i '' "s|\"Squirrel\", to: \\\\.distribution_code_name|\"${IME_BRAND_NAME}\", to: \\\\.distribution_code_name|g" sources/SquirrelApplicationDelegate.swift
    
    # Replace distribution_name - this is the display name in installation info
    sed -i '' "s|\"鼠鬚管\", to: \\\\.distribution_name|\"${IME_BRAND_NAME}\", to: \\\\.distribution_name|g" sources/SquirrelApplicationDelegate.swift
    
    # Replace identifiers in Info.plist
    sed -i '' "s|Squirrel_Connection|${IME_BRAND_NAME}_Connection|g" resources/Info.plist
    
    # Update bundle name and identifier
    /usr/libexec/PlistBuddy -c "Set :CFBundleName ${IME_BRAND_NAME}" resources/Info.plist
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier ${IME_BRAND_IDENTIFIER}" resources/Info.plist
    /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable ${IME_BRAND_NAME}" resources/Info.plist
    /usr/libexec/PlistBuddy -c "Set :TISInputSourceID ${IME_BRAND_IDENTIFIER}" resources/Info.plist
    
    # Update InputMethodServerControllerClass and InputMethodServerDelegateClass
    # These must match the Swift module name (PRODUCT_NAME) + class name
    # Format: "ModuleName.ClassName"
    /usr/libexec/PlistBuddy -c "Set :InputMethodServerControllerClass ${IME_BRAND_NAME}.SquirrelInputController" resources/Info.plist
    /usr/libexec/PlistBuddy -c "Set :InputMethodServerDelegateClass ${IME_BRAND_NAME}.SquirrelInputController" resources/Info.plist
    
    # Disable Sparkle auto-update (we'll use our own mechanism)
    /usr/libexec/PlistBuddy -c "Set :SUEnableAutomaticChecks false" resources/Info.plist 2>/dev/null || true
    
    # ============================================
    # Update ComponentInputModeDict with new identifiers
    # This is critical for macOS to recognize the input source
    # ============================================
    PLIST_FILE="resources/Info.plist"
    OLD_HANS_ID="im.rime.inputmethod.Squirrel.Hans"
    OLD_HANT_ID="im.rime.inputmethod.Squirrel.Hant"
    NEW_HANS_ID="${IME_BRAND_IDENTIFIER}.Hans"
    NEW_HANT_ID="${IME_BRAND_IDENTIFIER}.Hant"
    
    echo "Updating ComponentInputModeDict..."
    echo "  Hans: ${OLD_HANS_ID} -> ${NEW_HANS_ID}"
    echo "  Hant: ${OLD_HANT_ID} -> ${NEW_HANT_ID}"
    
    # Create new Hans entry by copying old one
    /usr/libexec/PlistBuddy -c "Copy :ComponentInputModeDict:tsInputModeListKey:${OLD_HANS_ID} :ComponentInputModeDict:tsInputModeListKey:${NEW_HANS_ID}" "${PLIST_FILE}"
    /usr/libexec/PlistBuddy -c "Set :ComponentInputModeDict:tsInputModeListKey:${NEW_HANS_ID}:TISInputSourceID ${NEW_HANS_ID}" "${PLIST_FILE}"
    /usr/libexec/PlistBuddy -c "Delete :ComponentInputModeDict:tsInputModeListKey:${OLD_HANS_ID}" "${PLIST_FILE}"
    
    # Create new Hant entry by copying old one
    /usr/libexec/PlistBuddy -c "Copy :ComponentInputModeDict:tsInputModeListKey:${OLD_HANT_ID} :ComponentInputModeDict:tsInputModeListKey:${NEW_HANT_ID}" "${PLIST_FILE}"
    /usr/libexec/PlistBuddy -c "Set :ComponentInputModeDict:tsInputModeListKey:${NEW_HANT_ID}:TISInputSourceID ${NEW_HANT_ID}" "${PLIST_FILE}"
    /usr/libexec/PlistBuddy -c "Delete :ComponentInputModeDict:tsInputModeListKey:${OLD_HANT_ID}" "${PLIST_FILE}"
    
    # Update tsVisibleInputModeOrderedArrayKey
    /usr/libexec/PlistBuddy -c "Set :ComponentInputModeDict:tsVisibleInputModeOrderedArrayKey:0 ${NEW_HANS_ID}" "${PLIST_FILE}"
    /usr/libexec/PlistBuddy -c "Set :ComponentInputModeDict:tsVisibleInputModeOrderedArrayKey:1 ${NEW_HANT_ID}" "${PLIST_FILE}"
    
    echo "Brand customization applied."
    echo ""
    
    # Clean previous build to ensure Info.plist changes take effect
    echo "Cleaning previous build..."
    rm -rf build/Build/Products
    rm -rf build/Build/Intermediates.noindex
    
    # Build Squirrel
    mkdir -p build
    bash package/add_data_files
    
    # Build with IME integration libraries
    # Note: We need to link our static libraries, SQLite3, and C++ runtime
    IME_LIBS="-L${PWD}/ime_integration/lib -lime_platform_macos -lime_learning -lime_input -lime_frequency -lime_storage -lsqlite3 -lc++"
    IME_HEADER_SEARCH_PATHS="${PWD}/ime_integration/include"
    
    xcodebuild -project Squirrel.xcodeproj \
        -configuration Release \
        -scheme Squirrel \
        -derivedDataPath build \
        COMPILER_INDEX_STORE_ENABLE=YES \
        PRODUCT_NAME="${IME_BRAND_NAME}" \
        PRODUCT_BUNDLE_IDENTIFIER="${IME_BRAND_IDENTIFIER}" \
        INFOPLIST_FILE="${PWD}/resources/Info.plist" \
        OTHER_LDFLAGS="-lrime.1 ${IME_LIBS}" \
        HEADER_SEARCH_PATHS="\$(inherited) ${IME_HEADER_SEARCH_PATHS}" \
        build
    
    BUILD_RESULT=$?
    
    # ============================================
    # Restore original source files
    # ============================================
    echo ""
    echo "Restoring original source files..."
    mv sources/InputSource.swift.bak sources/InputSource.swift
    mv sources/Main.swift.bak sources/Main.swift
    mv sources/SquirrelApplicationDelegate.swift.bak sources/SquirrelApplicationDelegate.swift
    mv resources/Info.plist.bak resources/Info.plist
    
    if [ $BUILD_RESULT -ne 0 ]; then
        echo "ERROR: Failed to build app!"
        exit 1
    fi
    
    # ============================================
    # Post-build: Update localization files in build output
    # This must be done AFTER xcodebuild because Xcode copies from source
    # ============================================
    echo "Updating localization files in build output..."
    BUILD_APP_PATH="build/Build/Products/Release/${IME_BRAND_NAME}.app"
    for lproj in "${BUILD_APP_PATH}/Contents/Resources/"*.lproj; do
        strings_file="$lproj/InfoPlist.strings"
        if [ -f "$strings_file" ]; then
            # Convert to XML for editing
            plutil -convert xml1 "$strings_file" 2>/dev/null || true
            # Replace old input source IDs with new ones
            sed -i '' "s|im\.rime\.inputmethod\.Squirrel|${IME_BRAND_IDENTIFIER}|g" "$strings_file"
            # Replace display names (Squirrel/鼠须管/鼠鬚管 -> brand name)
            sed -i '' "s|>鼠须管<|>${IME_BRAND_NAME}<|g" "$strings_file"
            sed -i '' "s|>鼠鬚管<|>${IME_BRAND_NAME}<|g" "$strings_file"
            sed -i '' "s|>Squirrel<|>${IME_BRAND_NAME}<|g" "$strings_file"
            echo "  Updated: $strings_file"
        fi
    done
    
    # Re-sign the app after modifying resources
    echo "Re-signing app..."
    codesign --force --deep --sign - "${BUILD_APP_PATH}"
    
    # ============================================
    # Copy custom Rime configurations to build output
    # ============================================
    if [ -d "${PROJECT_ROOT}/data/rime" ]; then
        echo "Copying custom Rime configurations to build output..."
        cp -R "${PROJECT_ROOT}/data/rime/"* "${BUILD_APP_PATH}/Contents/SharedSupport/"
        echo "  - Custom schemas and configurations applied"
    fi
    
    # Copy rime-ice dictionaries to build output
    if [ -d "${PROJECT_ROOT}/deps/rime-ice/cn_dicts" ]; then
        echo "Copying rime-ice Chinese dictionaries to build output..."
        mkdir -p "${BUILD_APP_PATH}/Contents/SharedSupport/cn_dicts"
        cp -R "${PROJECT_ROOT}/deps/rime-ice/cn_dicts/"* "${BUILD_APP_PATH}/Contents/SharedSupport/cn_dicts/"
    fi
    
    if [ -d "${PROJECT_ROOT}/deps/rime-ice/en_dicts" ]; then
        echo "Copying rime-ice English dictionaries to build output..."
        mkdir -p "${BUILD_APP_PATH}/Contents/SharedSupport/en_dicts"
        cp -R "${PROJECT_ROOT}/deps/rime-ice/en_dicts/"* "${BUILD_APP_PATH}/Contents/SharedSupport/en_dicts/"
    fi
    
    echo "App built successfully."
fi

# Build package
if [ $BUILD_PACKAGE -eq 1 ]; then
    echo ""
    echo "============================================"
    echo "Building Package"
    echo "============================================"
    echo ""
    
    cd "${PROJECT_ROOT}/installer/macos/package"
    chmod +x make_package.sh
    
    ./make_package.sh "${PROJECT_ROOT}/deps/squirrel/build"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to build package!"
        exit 1
    fi
    
    echo "Package built successfully."
fi

# Sign and notarize
if [ $BUILD_SIGN -eq 1 ]; then
    echo ""
    echo "============================================"
    echo "Signing and Notarizing"
    echo "============================================"
    echo ""
    
    if [ -z "${DEV_ID}" ]; then
        echo "ERROR: DEV_ID environment variable not set!"
        echo "Please set DEV_ID to your Apple Developer ID."
        exit 1
    fi
    
    cd "${PROJECT_ROOT}/installer/macos/package"
    chmod +x sign_and_notarize.sh
    
    ./sign_and_notarize.sh "${DEV_ID}" "${PROJECT_ROOT}/deps/squirrel/build"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to sign and notarize!"
        exit 1
    fi
    
    echo "Signing and notarization completed."
fi

echo ""
echo "============================================"
echo "Build completed successfully!"
echo "============================================"
echo ""
echo "Output files:"
ls -la "${PROJECT_ROOT}/output/macos/"

cd "${PROJECT_ROOT}"
