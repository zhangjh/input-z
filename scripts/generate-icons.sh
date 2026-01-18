#!/bin/bash
# Icon Generation Script for SuYan Input Method
#
# This script generates all required icon assets from source image files.
#
# Usage:
#   ./scripts/generate-icons.sh
#
# Prerequisites:
#   - sips (built-in macOS tool for PNG resizing)
#   - iconutil (built-in macOS tool for .icns generation)
#   - For SVG support: brew install librsvg
#
# Input files (place in resources/icons/):
#   - app-icon.svg  - Application icon (1024x1024)
#   - menubar-icon.png or .svg      - Menu bar icon (single color, template style)
#   - status-cn.png or .svg         - Chinese input status icon (optional)
#   - status-en.png or .svg         - English input status icon (optional)
#
# Output:
#   - resources/icons/generated/SuYan.icns  - macOS app icon
#   - resources/icons/generated/menubar/    - Menu bar icons (@1x, @2x, @3x)
#   - resources/icons/generated/status/     - Input status icons

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Icon source and output directories
ICON_SRC_DIR="${PROJECT_ROOT}/resources/icons"
ICON_OUT_DIR="${PROJECT_ROOT}/resources/icons/generated"
ICONSET_DIR="${ICON_OUT_DIR}/SuYan.iconset"

# Create directories
mkdir -p "${ICON_SRC_DIR}"
mkdir -p "${ICON_OUT_DIR}"
mkdir -p "${ICONSET_DIR}"
mkdir -p "${ICON_OUT_DIR}/menubar"
mkdir -p "${ICON_OUT_DIR}/status"

# Detect SVG converter (optional, for SVG files)
SVG_CONVERTER=""
if command -v rsvg-convert &> /dev/null; then
    SVG_CONVERTER="rsvg"
elif command -v inkscape &> /dev/null; then
    SVG_CONVERTER="inkscape"
elif command -v /Applications/Inkscape.app/Contents/MacOS/inkscape &> /dev/null; then
    SVG_CONVERTER="inkscape-app"
fi

# Function to resize PNG using sips (macOS built-in)
resize_png() {
    local input="$1"
    local output="$2"
    local size="$3"
    
    # Copy and resize
    cp "${input}" "${output}"
    sips -z "${size}" "${size}" "${output}" > /dev/null 2>&1
}

# Function to convert SVG to PNG
svg_to_png() {
    local input="$1"
    local output="$2"
    local size="$3"
    
    case "${SVG_CONVERTER}" in
        rsvg)
            rsvg-convert -w "${size}" -h "${size}" "${input}" -o "${output}"
            ;;
        inkscape)
            inkscape "${input}" --export-filename="${output}" -w "${size}" -h "${size}" 2>/dev/null
            ;;
        inkscape-app)
            /Applications/Inkscape.app/Contents/MacOS/inkscape "${input}" --export-filename="${output}" -w "${size}" -h "${size}" 2>/dev/null
            ;;
        *)
            echo -e "${RED}Error: SVG converter not available${NC}"
            return 1
            ;;
    esac
}

# Function to convert image to target size (supports PNG and SVG)
convert_image() {
    local input="$1"
    local output="$2"
    local size="$3"
    
    local ext="${input##*.}"
    
    case "${ext}" in
        png|PNG)
            resize_png "${input}" "${output}" "${size}"
            ;;
        svg|SVG)
            svg_to_png "${input}" "${output}" "${size}"
            ;;
        *)
            echo -e "${RED}Error: Unsupported format ${ext}${NC}"
            return 1
            ;;
    esac
}

# Function to find source file (prefers SVG for vector quality, falls back to PNG)
find_source() {
    local basename="$1"
    
    # Prefer SVG for better quality (especially for PDF generation)
    if [ -f "${ICON_SRC_DIR}/${basename}.svg" ]; then
        echo "${ICON_SRC_DIR}/${basename}.svg"
    elif [ -f "${ICON_SRC_DIR}/${basename}.png" ]; then
        echo "${ICON_SRC_DIR}/${basename}.png"
    else
        echo ""
    fi
}

echo ""
echo "============================================"
echo "  SuYan Icon Generator"
echo "============================================"
echo ""

# ============================================
# Generate Application Icon (.icns)
# ============================================
APP_ICON_SRC=$(find_source "app-icon")

if [ -n "${APP_ICON_SRC}" ]; then
    echo -e "${GREEN}Generating application icon from: ${APP_ICON_SRC##*/}${NC}"
    
    # macOS .icns requires these sizes
    ICON_SIZES=(16 32 64 128 256 512 1024)
    
    for size in "${ICON_SIZES[@]}"; do
        echo "  - ${size}x${size}"
        convert_image "${APP_ICON_SRC}" "${ICONSET_DIR}/icon_${size}x${size}.png" "${size}"
        
        # Also generate @2x versions (except for 1024 which is already @2x of 512)
        if [ "${size}" -lt 512 ]; then
            double=$((size * 2))
            convert_image "${APP_ICON_SRC}" "${ICONSET_DIR}/icon_${size}x${size}@2x.png" "${double}"
        fi
    done
    
    # Generate .icns file
    echo "  - Creating SuYan.icns"
    iconutil -c icns "${ICONSET_DIR}" -o "${ICON_OUT_DIR}/SuYan.icns"
    
    echo -e "${GREEN}  ✓ Application icon generated${NC}"
else
    echo -e "${YELLOW}Warning: app-icon.svg not found, skipping application icon${NC}"
    echo "  Expected: ${ICON_SRC_DIR}/app-icon.svg"
fi

# ============================================
# Generate Menu Bar Icons
# ============================================
MENUBAR_ICON_SRC=$(find_source "menubar-icon")

# If no dedicated menubar icon, use app icon
if [ -z "${MENUBAR_ICON_SRC}" ]; then
    MENUBAR_ICON_SRC=$(find_source "app-icon")
    if [ -n "${MENUBAR_ICON_SRC}" ]; then
        echo ""
        echo -e "${YELLOW}No menubar-icon found, using app-icon instead${NC}"
    fi
fi

if [ -n "${MENUBAR_ICON_SRC}" ]; then
    echo ""
    echo -e "${GREEN}Generating menu bar icons from: ${MENUBAR_ICON_SRC##*/}${NC}"
    
    # Menu bar icon sizes (18pt base)
    echo "  - menubar.png (18x18)"
    convert_image "${MENUBAR_ICON_SRC}" "${ICON_OUT_DIR}/menubar/menubar.png" 18
    
    echo "  - menubar@2x.png (36x36)"
    convert_image "${MENUBAR_ICON_SRC}" "${ICON_OUT_DIR}/menubar/menubar@2x.png" 36
    
    echo "  - menubar@3x.png (54x54)"
    convert_image "${MENUBAR_ICON_SRC}" "${ICON_OUT_DIR}/menubar/menubar@3x.png" 54
    
    # Generate rime.pdf for menu bar (required by Info.plist)
    # This is the icon shown in the menu bar and input source list
    # IMPORTANT: Must be a VECTOR PDF with correct page size!
    # Original Squirrel uses 22x16 points (from Adobe Illustrator)
    # The PDF page size determines how large the icon appears in the menu bar
    MENUBAR_PDF_WIDTH=22   # points (matching original Squirrel)
    MENUBAR_PDF_HEIGHT=16  # points
    
    echo "  - rime.pdf (menu bar icon - ${MENUBAR_PDF_WIDTH}x${MENUBAR_PDF_HEIGHT}pt vector PDF)"
    
    MENUBAR_SRC_EXT="${MENUBAR_ICON_SRC##*.}"
    if [ "${MENUBAR_SRC_EXT}" = "svg" ] || [ "${MENUBAR_SRC_EXT}" = "SVG" ]; then
        # SVG source: convert directly to vector PDF using rsvg-convert
        # Use --page-width and --page-height to set the PDF page size
        # Use -w and -h to scale the content to fit
        if command -v rsvg-convert &> /dev/null; then
            rsvg-convert -f pdf \
                --page-width "${MENUBAR_PDF_WIDTH}" \
                --page-height "${MENUBAR_PDF_HEIGHT}" \
                -w "${MENUBAR_PDF_WIDTH}" \
                -h "${MENUBAR_PDF_HEIGHT}" \
                --keep-aspect-ratio \
                "${MENUBAR_ICON_SRC}" \
                -o "${ICON_OUT_DIR}/menubar/rime.pdf"
            echo "    (converted from SVG to ${MENUBAR_PDF_WIDTH}x${MENUBAR_PDF_HEIGHT}pt vector PDF)"
        else
            echo -e "${RED}Error: rsvg-convert not found! Cannot create vector PDF from SVG.${NC}"
            echo "    Please install librsvg: brew install librsvg"
            # Fallback to bitmap PDF (not ideal but better than nothing)
            convert_image "${MENUBAR_ICON_SRC}" "${ICON_OUT_DIR}/menubar/temp_hires.png" 256
            sips -s format pdf "${ICON_OUT_DIR}/menubar/temp_hires.png" --out "${ICON_OUT_DIR}/menubar/rime.pdf" > /dev/null 2>&1
            rm -f "${ICON_OUT_DIR}/menubar/temp_hires.png"
            echo -e "${YELLOW}    Warning: Created bitmap PDF (not ideal for menu bar)${NC}"
        fi
    else
        # PNG source: can only create bitmap PDF
        echo -e "${YELLOW}    Warning: PNG source can only create bitmap PDF${NC}"
        echo "    For best results, provide an SVG source file"
        convert_image "${MENUBAR_ICON_SRC}" "${ICON_OUT_DIR}/menubar/temp_hires.png" 256
        sips -s format pdf "${ICON_OUT_DIR}/menubar/temp_hires.png" --out "${ICON_OUT_DIR}/menubar/rime.pdf" > /dev/null 2>&1
        rm -f "${ICON_OUT_DIR}/menubar/temp_hires.png"
    fi
    
    echo -e "${GREEN}  ✓ Menu bar icons generated${NC}"
else
    echo ""
    echo -e "${YELLOW}Warning: No icon source found for menu bar${NC}"
fi

# ============================================
# Generate Input Status Icons (Optional)
# ============================================
echo ""
echo "Checking for input status icons..."

STATUS_ICONS=("status-cn:中文" "status-en:英文" "status-hant:繁體")
STATUS_FOUND=0

for item in "${STATUS_ICONS[@]}"; do
    name="${item%%:*}"
    desc="${item##*:}"
    src_file=$(find_source "${name}")
    
    if [ -n "${src_file}" ]; then
        echo -e "${GREEN}  Generating ${name} (${desc})${NC}"
        convert_image "${src_file}" "${ICON_OUT_DIR}/status/${name}.png" 18
        convert_image "${src_file}" "${ICON_OUT_DIR}/status/${name}@2x.png" 36
        convert_image "${src_file}" "${ICON_OUT_DIR}/status/${name}@3x.png" 54
        STATUS_FOUND=1
    fi
done

if [ "${STATUS_FOUND}" -eq 0 ]; then
    echo -e "${YELLOW}  No status icons found (optional)${NC}"
fi

echo ""
echo "============================================"
echo "  Icon Generation Complete"
echo "============================================"
echo ""
echo "Output directory: ${ICON_OUT_DIR}"
echo ""

# List generated files
if [ -d "${ICON_OUT_DIR}" ]; then
    echo "Generated files:"
    find "${ICON_OUT_DIR}" -type f \( -name "*.png" -o -name "*.icns" -o -name "*.pdf" \) | sort | while read f; do
        echo "  - ${f#${ICON_OUT_DIR}/}"
    done
fi

# ============================================
# Create Rime.icon directory for Xcode
# ============================================
RIME_ICON_DIR="${ICON_OUT_DIR}/Rime.icon"
RIME_ICON_ASSETS="${RIME_ICON_DIR}/Assets"

echo ""
echo -e "${GREEN}Creating Rime.icon for Xcode...${NC}"

mkdir -p "${RIME_ICON_ASSETS}"

# Copy app icon as logo.png (Xcode will use this)
if [ -f "${ICONSET_DIR}/icon_1024x1024.png" ]; then
    cp "${ICONSET_DIR}/icon_1024x1024.png" "${RIME_ICON_ASSETS}/logo.png"
    echo "  - Copied logo.png"
fi

# Create icon.json configuration
cat > "${RIME_ICON_DIR}/icon.json" << 'EOF'
{
  "color-space-for-untagged-svg-colors" : "display-p3",
  "groups" : [
    {
      "blur-material" : null,
      "layers" : [
        {
          "fill-specializations" : [
            {
              "appearance" : "dark",
              "value" : {
                "solid" : "gray:1.00000,1.00000"
              }
            },
            {
              "appearance" : "tinted",
              "value" : {
                "solid" : "extended-gray:1.00000,1.00000"
              }
            }
          ],
          "glass" : true,
          "image-name" : "logo.png",
          "name" : "logo"
        }
      ],
      "shadow" : {
        "kind" : "neutral",
        "opacity" : 0.5
      },
      "specular" : true,
      "translucency" : {
        "enabled" : false,
        "value" : 0.5
      }
    }
  ],
  "supported-platforms" : {
    "squares" : [
      "macOS"
    ]
  }
}
EOF
echo "  - Created icon.json"
echo -e "${GREEN}  ✓ Rime.icon created${NC}"

echo ""
echo -e "${GREEN}Next steps:${NC}"
echo "The build script will automatically use these icons."
echo ""
echo "To manually copy icons to Squirrel:"
echo "  cp -R ${RIME_ICON_DIR} deps/squirrel/Rime.icon"
echo "  cp ${ICON_OUT_DIR}/menubar/* deps/squirrel/resources/"
