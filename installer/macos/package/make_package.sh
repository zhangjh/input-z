#!/bin/bash
# macOS Package Builder
#
# This script creates a macOS installer package (.pkg).
#
# Usage:
#   ./make_package.sh [derived_data_path]
#
# Arguments:
#   derived_data_path: Path to Xcode derived data (default: build)
#
# Environment Variables:
#   IME_BRAND_NAME        - App display name (default: InputZ)
#   IME_BRAND_IDENTIFIER  - Bundle identifier (default: cn.zhangjh.inputmethod.InputZ)
#                           NOTE: Must contain "inputmethod" for macOS recognition
#
# Examples:
#   ./make_package.sh
#   IME_BRAND_NAME=智拼 IME_BRAND_IDENTIFIER=cn.zhangjh.zhipin ./make_package.sh
#
# Prerequisites:
#   - Xcode Command Line Tools
#   - Built Squirrel.app in derived data path

set -e

DERIVED_DATA_PATH="${1:-build}"

# ============================================
# Brand Configuration - Read from environment
# ============================================
BRAND_NAME="${IME_BRAND_NAME:-InputZ}"
# Note: The identifier MUST contain "inputmethod" for macOS to recognize it
BRAND_IDENTIFIER="${IME_BRAND_IDENTIFIER:-cn.zhangjh.inputmethod.InputZ}"
BRAND_INPUT_SOURCE_ID="${BRAND_IDENTIFIER}"
BRAND_INPUT_SOURCE_ID="${BRAND_IDENTIFIER}"
INSTALL_LOCATION='/Library/Input Methods'

# Configurable options
REQUIRE_LOGOUT="${IME_REQUIRE_LOGOUT:-false}"
AUTO_OPEN_SETTINGS="${IME_AUTO_OPEN_SETTINGS:-true}"
PREBUILD_RIME="${IME_PREBUILD_RIME:-true}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# Source common functions
source "${SCRIPT_DIR}/common.sh"

echo "============================================"
echo "${BRAND_NAME} macOS Package Builder"
echo "============================================"
echo ""
echo "Project Root: ${PROJECT_ROOT}"
echo "Derived Data: ${DERIVED_DATA_PATH}"
echo "Brand: ${BRAND_NAME} (${BRAND_IDENTIFIER})"
echo ""

# Get version
APP_VERSION=$(get_app_version)
echo "Version: ${APP_VERSION}"
echo ""

# Handle absolute vs relative path
if [[ "${DERIVED_DATA_PATH}" = /* ]]; then
    BUILD_ROOT="${DERIVED_DATA_PATH}"
else
    BUILD_ROOT="${PROJECT_ROOT}/${DERIVED_DATA_PATH}"
fi

# Check for built app
RELEASE_DIR="${BUILD_ROOT}/Build/Products/Release"
APP_PATH="${RELEASE_DIR}/${BRAND_NAME}.app"

# Check if branded app already exists (built with brand customization)
if [ -d "${APP_PATH}" ]; then
    echo "Found ${BRAND_NAME}.app (already branded)"
else
    # Try Squirrel.app as fallback (for development without brand customization)
    SQUIRREL_PATH="${RELEASE_DIR}/Squirrel.app"
    if [ -d "${SQUIRREL_PATH}" ]; then
        echo "Found Squirrel.app, copying and rebranding..."
        cp -R "${SQUIRREL_PATH}" "${APP_PATH}"
        
        INFO_PLIST="${APP_PATH}/Contents/Info.plist"
        
        echo "Applying brand customization to copied app..."
        
        # Basic bundle info
        /usr/libexec/PlistBuddy -c "Set :CFBundleName ${BRAND_NAME}" "${INFO_PLIST}"
        /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName ${BRAND_NAME}" "${INFO_PLIST}" 2>/dev/null || \
            /usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string ${BRAND_NAME}" "${INFO_PLIST}"
        /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier ${BRAND_IDENTIFIER}" "${INFO_PLIST}"
        /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable ${BRAND_NAME}" "${INFO_PLIST}"
        
        # Rename executable
        if [ -f "${APP_PATH}/Contents/MacOS/Squirrel" ]; then
            mv "${APP_PATH}/Contents/MacOS/Squirrel" "${APP_PATH}/Contents/MacOS/${BRAND_NAME}"
        fi
    else
        echo "ERROR: Built app not found at expected location."
        echo "Expected: ${APP_PATH} or ${SQUIRREL_PATH}"
        echo ""
        echo "Please build the app first using:"
        echo "  ./scripts/build-macos-installer.sh app"
        exit 1
    fi
fi

APP_NAME="${BRAND_NAME}.app"
echo "App Path: ${APP_PATH}"
echo "App Name: ${APP_NAME}"
echo ""

# Create output directory
OUTPUT_DIR="${PROJECT_ROOT}/output/macos"
mkdir -p "${OUTPUT_DIR}"

# Generate component plist dynamically based on actual app name
TEMP_COMPONENT_PLIST="${OUTPUT_DIR}/component.plist"
cat > "${TEMP_COMPONENT_PLIST}" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
	<dict>
		<key>RootRelativeBundlePath</key>
		<string>${APP_NAME}</string>
		<key>BundleHasStrictIdentifier</key>
		<false/>
		<key>BundleIsRelocatable</key>
		<false/>
		<key>BundleIsVersionChecked</key>
		<true/>
		<key>BundleOverwriteAction</key>
		<string>upgrade</string>
		<key>ChildBundles</key>
		<array>
			<dict>
				<key>BundleOverwriteAction</key>
				<string>upgrade</string>
				<key>RootRelativeBundlePath</key>
				<string>${APP_NAME}/Contents/Frameworks/Sparkle.framework</string>
			</dict>
		</array>
	</dict>
</array>
</plist>
EOF

echo "Generated component plist for: ${APP_NAME}"
echo ""

# Generate install scripts with brand configuration embedded
SCRIPTS_DIR="${OUTPUT_DIR}/scripts"
mkdir -p "${SCRIPTS_DIR}"

# Generate preinstall script
cat > "${SCRIPTS_DIR}/preinstall" << 'PREINSTALL_HEADER'
#!/bin/bash
# macOS Pre-Installation Script (Auto-generated)

set -e
PREINSTALL_HEADER

cat >> "${SCRIPTS_DIR}/preinstall" << PREINSTALL_BRAND
# Brand configuration (embedded at build time)
BRAND_NAME="${BRAND_NAME}"
PREINSTALL_BRAND

cat >> "${SCRIPTS_DIR}/preinstall" << 'PREINSTALL_BODY'

echo "${BRAND_NAME} Pre-Installation"
echo "==========================="

# Get the current logged-in user
login_user=$(/usr/bin/stat -f%Su /dev/console)
echo "User: ${login_user}"

# Check macOS version (minimum 13.0 Ventura)
os_version=$(sw_vers -productVersion)
major_version=$(echo "${os_version}" | cut -d. -f1)

echo "macOS Version: ${os_version}"

if [ "${major_version}" -lt 13 ]; then
    echo "ERROR: ${BRAND_NAME} requires macOS 13.0 (Ventura) or later."
    echo "Your version: ${os_version}"
    exit 1
fi

# Define paths
user_data_dir="/Users/${login_user}/Library/Rime"
backup_dir="/tmp/${BRAND_NAME}-backup-$(date +%Y%m%d%H%M%S)"

# Stop any running IME process
echo "Stopping existing IME processes..."
/usr/bin/sudo -u "${login_user}" /usr/bin/killall "${BRAND_NAME}" > /dev/null 2>&1 || true
/usr/bin/sudo -u "${login_user}" /usr/bin/killall Squirrel > /dev/null 2>&1 || true

# Backup user data if exists
if [ -d "${user_data_dir}" ]; then
    echo "Backing up user data to ${backup_dir}..."
    mkdir -p "${backup_dir}"
    cp -R "${user_data_dir}" "${backup_dir}/"
    echo "Backup completed."
fi

echo "Pre-installation completed successfully!"
PREINSTALL_BODY

chmod +x "${SCRIPTS_DIR}/preinstall"

# Generate postinstall script
cat > "${SCRIPTS_DIR}/postinstall" << 'POSTINSTALL_HEADER'
#!/bin/bash
# macOS Post-Installation Script (Auto-generated)

set -e
POSTINSTALL_HEADER

cat >> "${SCRIPTS_DIR}/postinstall" << POSTINSTALL_BRAND
# Brand configuration (embedded at build time)
BRAND_NAME="${BRAND_NAME}"
REQUIRE_LOGOUT="${REQUIRE_LOGOUT}"
AUTO_OPEN_SETTINGS="${AUTO_OPEN_SETTINGS}"
PREBUILD_RIME="${PREBUILD_RIME}"
POSTINSTALL_BRAND

cat >> "${SCRIPTS_DIR}/postinstall" << 'POSTINSTALL_BODY'

# Get the current logged-in user
login_user=$(/usr/bin/stat -f%Su /dev/console)

# Define paths
ime_app_root="/Library/Input Methods/${BRAND_NAME}.app"
ime_executable="${ime_app_root}/Contents/MacOS/${BRAND_NAME}"

# Fallback to Squirrel.app if branded app not found
if [ ! -d "${ime_app_root}" ]; then
    ime_app_root="/Library/Input Methods/Squirrel.app"
    ime_executable="${ime_app_root}/Contents/MacOS/Squirrel"
fi

rime_shared_data_path="${ime_app_root}/Contents/SharedSupport"

echo "${BRAND_NAME} Post-Installation"
echo "============================"
echo "User: ${login_user}"
echo "App Root: ${ime_app_root}"
echo "Executable: ${ime_executable}"

# Kill any existing IME process
echo "Stopping existing IME process..."
/usr/bin/sudo -u "${login_user}" /usr/bin/killall "${BRAND_NAME}" > /dev/null 2>&1 || true
/usr/bin/sudo -u "${login_user}" /usr/bin/killall Squirrel > /dev/null 2>&1 || true

# Wait a moment for process to terminate
sleep 1

# Check if executable exists
if [ ! -f "${ime_executable}" ]; then
    echo "WARNING: Executable not found at ${ime_executable}"
    echo "Skipping input source registration. Please restart your Mac to activate the input method."
    exit 0
fi

# Register the input source
echo "Registering input source..."
"${ime_executable}" --register-input-source || true

# Pre-build RIME data unless disabled
if [ "${PREBUILD_RIME}" = "true" ] && [ -z "${RIME_NO_PREBUILD}" ]; then
    echo "Pre-building RIME data..."
    
    # Clean user RIME build cache to avoid conflicts with other RIME-based IMEs
    # This ensures the new IME generates fresh build artifacts with correct distribution info
    user_rime_dir="/Users/${login_user}/Library/Rime"
    if [ -d "${user_rime_dir}/build" ]; then
        echo "Cleaning user RIME build cache..."
        rm -rf "${user_rime_dir}/build"
    fi
    
    # Remove old installation.yaml if it exists from a different RIME distribution
    # This prevents conflicts between InputZ and other RIME IMEs (like Squirrel)
    if [ -f "${user_rime_dir}/installation.yaml" ]; then
        if grep -q "distribution_code_name:" "${user_rime_dir}/installation.yaml" 2>/dev/null; then
            old_distro=$(grep "distribution_code_name:" "${user_rime_dir}/installation.yaml" | cut -d: -f2 | tr -d ' ')
            if [ "${old_distro}" != "${BRAND_NAME}" ]; then
                echo "Detected old RIME distribution (${old_distro}), removing stale configuration..."
                rm -f "${user_rime_dir}/installation.yaml"
            fi
        fi
    fi
    
    if [ -d "${rime_shared_data_path}" ]; then
        pushd "${rime_shared_data_path}" > /dev/null
        "${ime_executable}" --build || true
        popd > /dev/null
    fi
fi

# Enable and select the input source
echo "Enabling input source..."
/usr/bin/sudo -u "${login_user}" "${ime_executable}" --enable-input-source || true
/usr/bin/sudo -u "${login_user}" "${ime_executable}" --select-input-source || true

echo "Post-installation completed successfully!"
echo ""

if [ "${REQUIRE_LOGOUT}" = "true" ]; then
    echo "============================================"
    echo "IMPORTANT: Please log out and log back in"
    echo "to activate ${BRAND_NAME} input method."
    echo "============================================"
    
    # Show a dialog to remind user to log out
    /usr/bin/osascript -e "display dialog \"${BRAND_NAME} 安装完成！\n\n请注销并重新登录以激活输入法。\" buttons {\"好的\"} default button 1 with title \"${BRAND_NAME} 安装程序\" with icon note" 2>/dev/null || true
else
    echo "============================================"
    echo "${BRAND_NAME} installed successfully."
    echo "Please add it in System Settings > Keyboard > Input Sources."
    echo "============================================"
    
    if [ "${AUTO_OPEN_SETTINGS}" = "true" ]; then
        echo "Opening System Settings..."
        /usr/bin/sudo -u "${login_user}" /usr/bin/open "x-apple.systempreferences:com.apple.Keyboard-Settings.extension" || true
        
        # Show a gentler dialog
        /usr/bin/osascript -e "display dialog \"${BRAND_NAME} 安装完成！\n\n如果输入法未自动出现，请在稍后打开的系统设置中点击 '+' 号手动添加。\" buttons {\"好的\"} default button 1 with title \"${BRAND_NAME} 安装程序\" with icon note" 2>/dev/null || true
    fi
fi
POSTINSTALL_BODY

chmod +x "${SCRIPTS_DIR}/postinstall"

echo "Generated install scripts with brand: ${BRAND_NAME}"
echo ""

echo "Generated install scripts with brand: ${BRAND_NAME}"
echo ""

# Generate PackageInfo dynamically
PACKAGE_INFO_PATH="${OUTPUT_DIR}/PackageInfo"
echo "Generating PackageInfo..."
echo "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>" > "${PACKAGE_INFO_PATH}"

if [ "${REQUIRE_LOGOUT}" = "true" ]; then
    # Standard logout action
    echo "<pkg-info postinstall-action=\"logout\"/>" >> "${PACKAGE_INFO_PATH}"
    echo "  Added postinstall-action=\"logout\""
else
    # No action required (just close the installer)
    echo "<pkg-info format-version=\"2\"/>" >> "${PACKAGE_INFO_PATH}"
    echo "  No postinstall-action set"
fi

# Build the package
echo "Building package..."

pkgbuild \
    --info "${PACKAGE_INFO_PATH}" \
    --root "${BUILD_ROOT}/Build/Products/Release" \
    --filter '.*\.swiftmodule' \
    --filter '.*\.dSYM' \
    --filter 'Squirrel\.app' \
    --component-plist "${TEMP_COMPONENT_PLIST}" \
    --identifier "${BRAND_IDENTIFIER}" \
    --version "${APP_VERSION}" \
    --install-location "${INSTALL_LOCATION}" \
    --scripts "${SCRIPTS_DIR}" \
    "${OUTPUT_DIR}/${BRAND_NAME}-${APP_VERSION}.pkg"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build package!"
    exit 1
fi

echo ""
echo "============================================"
echo "Package built successfully!"
echo "Output: ${OUTPUT_DIR}/${BRAND_NAME}-${APP_VERSION}.pkg"
echo "============================================"
