#!/bin/bash
# Release Script - Create a new version tag and trigger GitHub Actions build
#
# Usage:
#   ./scripts/release.sh <version>
#
# Examples:
#   ./scripts/release.sh 1.0.0      # Creates tag v1.0.0
#   ./scripts/release.sh 1.0.0-beta # Creates tag v1.0.0-beta
#
# This script will:
#   1. Validate the version format
#   2. Check if the tag already exists
#   3. Create and push the tag to trigger GitHub Actions

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_ROOT}"

# Check arguments
if [ $# -eq 0 ]; then
    echo -e "${RED}Error: Version number required${NC}"
    echo ""
    echo "Usage: $0 <version>"
    echo ""
    echo "Examples:"
    echo "  $0 1.0.0"
    echo "  $0 1.0.0-beta"
    echo "  $0 1.0.0-rc1"
    exit 1
fi

VERSION="$1"
TAG_NAME="v${VERSION}"

# Validate version format (basic semver check)
if ! echo "${VERSION}" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+)?$'; then
    echo -e "${RED}Error: Invalid version format '${VERSION}'${NC}"
    echo ""
    echo "Version should follow semantic versioning: MAJOR.MINOR.PATCH[-PRERELEASE]"
    echo "Examples: 1.0.0, 1.0.0-beta, 1.0.0-rc1"
    exit 1
fi

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not a git repository${NC}"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD -- 2>/dev/null; then
    echo -e "${YELLOW}Warning: You have uncommitted changes${NC}"
    echo ""
    git status --short
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

# Check if tag already exists locally
if git tag -l | grep -q "^${TAG_NAME}$"; then
    echo -e "${RED}Error: Tag '${TAG_NAME}' already exists locally${NC}"
    echo ""
    echo "To delete the existing tag:"
    echo "  git tag -d ${TAG_NAME}"
    exit 1
fi

# Check if tag already exists on remote
if git ls-remote --tags origin | grep -q "refs/tags/${TAG_NAME}$"; then
    echo -e "${RED}Error: Tag '${TAG_NAME}' already exists on remote${NC}"
    echo ""
    echo "To delete the remote tag:"
    echo "  git push origin :refs/tags/${TAG_NAME}"
    exit 1
fi

# Load brand configuration for display
if [ -f "${PROJECT_ROOT}/brand.conf" ]; then
    set -a
    source "${PROJECT_ROOT}/brand.conf"
    set +a
fi
BRAND_NAME="${IME_BRAND_NAME:-SuYan}"

echo ""
echo "============================================"
echo "  ${BRAND_NAME} Release"
echo "============================================"
echo ""
echo "Version:  ${VERSION}"
echo "Tag:      ${TAG_NAME}"
echo "Branch:   $(git branch --show-current)"
echo "Commit:   $(git rev-parse --short HEAD)"
echo ""

# Confirm
read -p "Create and push tag '${TAG_NAME}'? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

# Create tag
echo ""
echo -e "${GREEN}Creating tag ${TAG_NAME}...${NC}"
git tag -a "${TAG_NAME}" -m "Release ${VERSION}"

# Push tag
echo -e "${GREEN}Pushing tag to origin...${NC}"
git push origin "${TAG_NAME}"

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Tag '${TAG_NAME}' created and pushed!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "GitHub Actions will now build the release."
echo ""
echo "View the build progress at:"
echo "  https://github.com/$(git remote get-url origin | sed -E 's|.*github.com[:/](.*)\.git|\1|')/actions"
echo ""
echo "Once complete, the release draft will be available at:"
echo "  https://github.com/$(git remote get-url origin | sed -E 's|.*github.com[:/](.*)\.git|\1|')/releases"
