#!/bin/bash
# IME Pinyin macOS Package Common Functions
#
# This file contains shared functions used by packaging scripts.

# Get project root directory
get_project_root() {
    cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd
}

PROJECT_ROOT="$(get_project_root)"

# Bump version number
# Usage: bump_version "1.0.0"
bump_version() {
    local version="$1"
    cd "${PROJECT_ROOT}"
    xcrun agvtool new-version "${version}"
}

# Get current app version from Xcode project
get_app_version() {
    # First check environment variable (for CI builds)
    if [ -n "${IME_VERSION}" ]; then
        echo "${IME_VERSION}"
        return
    fi
    
    cd "${PROJECT_ROOT}"
    # Try to get version from agvtool
    local version=$(xcrun agvtool what-version 2>/dev/null | sed -n 'n;s/^[[:space:]]*\([0-9.]*\)$/\1/;p')
    
    if [ -z "${version}" ]; then
        # Fallback to default version
        version="1.0.0"
    fi
    
    echo "${version}"
}

# Get bundle version from Info.plist
# Usage: get_bundle_version path/to/Info.plist
get_bundle_version() {
    sed -n '/CFBundleVersion/{n;s/.*<string>\(.*\)<\/string>.*/\1/;p;}' "$@"
}

# Check if a line matches a pattern
# Usage: match_line "pattern" file
match_line() {
    grep --quiet --fixed-strings "$@"
}

# Sign the app with Developer ID
# Usage: sign_app "Developer ID" "path/to/app"
sign_app() {
    local dev_id="$1"
    local app_path="$2"
    local entitlements="${PROJECT_ROOT}/installer/macos/resources/IMEPinyin.entitlements"
    
    if [ ! -f "${entitlements}" ]; then
        entitlements="${PROJECT_ROOT}/deps/squirrel/resources/Squirrel.entitlements"
    fi
    
    echo "Signing app: ${app_path}"
    echo "Developer ID: ${dev_id}"
    
    codesign --deep --force --options runtime --timestamp \
        --sign "Developer ID Application: ${dev_id}" \
        --entitlements "${entitlements}" \
        --verbose "${app_path}"
    
    # Verify signature
    spctl -a -vv "${app_path}"
}

# Notarize the package
# Usage: notarize_package "Developer ID" "path/to/package.pkg"
notarize_package() {
    local dev_id="$1"
    local pkg_path="$2"
    
    echo "Notarizing package: ${pkg_path}"
    
    xcrun notarytool submit "${pkg_path}" \
        --keychain-profile "${dev_id}" \
        --wait
    
    xcrun stapler staple "${pkg_path}"
}
