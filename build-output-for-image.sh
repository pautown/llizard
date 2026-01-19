#!/bin/bash

# Build llizardgui-host, plugins, and mediadash-client for ARM
# Outputs everything to a local folder for image building

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Navigate to project root first
cd "$(dirname "$0")"
PROJECT_ROOT="$(pwd)"

# Output directory (can be overridden with OUTPUT_DIR env var)
# Use absolute path so it works after cd into subdirectories
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_ROOT}/image-build-output}"

echo -e "${GREEN}=== Building llizardgui-host + plugins + mediadash-client for ARM ===${NC}"
echo -e "${YELLOW}Output directory: ${OUTPUT_DIR}${NC}"

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=""

if ! command -v cmake &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS cmake"
fi

if ! command -v arm-linux-gnueabihf-gcc &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS gcc-arm-linux-gnueabihf"
fi

if ! command -v go &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS golang-go"
fi

if [ -n "$MISSING_DEPS" ]; then
    echo -e "${RED}Missing dependencies:${NC}$MISSING_DEPS"
    echo -e "${YELLOW}Install with: sudo apt install$MISSING_DEPS${NC}"
    exit 1
fi

echo -e "${GREEN}All dependencies found!${NC}"

# Create output directory structure
echo -e "${YELLOW}Creating output directory structure...${NC}"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/bins"
mkdir -p "$OUTPUT_DIR/bins/plugins"
mkdir -p "$OUTPUT_DIR/fonts"
mkdir -p "$OUTPUT_DIR/data/flashcards/questions"
mkdir -p "$OUTPUT_DIR/data/millionaire/questions"

# ============================================================
# Build llizardgui-host and plugins
# ============================================================
echo -e "${GREEN}=== Building llizardgui-host (ARM/DRM) ===${NC}"

mkdir -p build-armv7-drm
cd build-armv7-drm

# Configure with ARM toolchain and DRM platform
echo -e "${YELLOW}Configuring CMake for ARM/DRM...${NC}"
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armv7.cmake -DPLATFORM=DRM ..

# Build everything
echo -e "${YELLOW}Building llizardgui-host and plugins...${NC}"
make -j$(nproc)

# Copy main executable
echo -e "${YELLOW}Copying llizardgui-host...${NC}"
cp llizardgui-host "$OUTPUT_DIR/bins/"

# Copy all plugins
echo -e "${YELLOW}Copying plugins...${NC}"
for plugin in *.so; do
    if [ -f "$plugin" ]; then
        echo "  - $plugin"
        cp "$plugin" "$OUTPUT_DIR/bins/plugins/"
    fi
done

cd "$PROJECT_ROOT"

# ============================================================
# Build mediadash-client (golang BLE client)
# ============================================================
echo -e "${GREEN}=== Building mediadash-client (ARM) ===${NC}"

cd supporting_projects/golang_ble_client

# Set cross-compilation environment for ARM
echo -e "${YELLOW}Setting up cross-compilation for ARMv7...${NC}"
export GOOS=linux
export GOARCH=arm
export GOARM=7
export CGO_ENABLED=0

# Create bin directory
mkdir -p bin

# Build the binary
echo -e "${YELLOW}Building mediadash-client...${NC}"
go build -o bin/mediadash-client ./cmd/mediadash-client

# Copy to output
echo -e "${YELLOW}Copying mediadash-client...${NC}"
cp bin/mediadash-client "$OUTPUT_DIR/bins/"

cd "$PROJECT_ROOT"

# ============================================================
# Copy fonts
# ============================================================
echo -e "${GREEN}=== Copying fonts ===${NC}"

if [ -d "fonts" ] && [ "$(ls -A fonts/*.ttf 2>/dev/null)" ]; then
    for font in fonts/*.ttf; do
        if [ -f "$font" ]; then
            fontname=$(basename "$font")
            echo "  - $fontname"
            cp "$font" "$OUTPUT_DIR/fonts/"
        fi
    done
else
    echo -e "${YELLOW}No fonts found in fonts/ directory${NC}"
fi

# ============================================================
# Copy plugin data (flashcards, millionaire questions, etc.)
# ============================================================
echo -e "${GREEN}=== Copying plugin data ===${NC}"

# Flashcards questions
if [ -d "plugins/flashcards/questions" ]; then
    echo -e "${YELLOW}Copying flashcards questions...${NC}"
    cp -r plugins/flashcards/questions/* "$OUTPUT_DIR/data/flashcards/questions/" 2>/dev/null || true
fi

# Millionaire questions
if [ -d "plugins/millionaire/questions" ]; then
    echo -e "${YELLOW}Copying millionaire questions...${NC}"
    cp -r plugins/millionaire/questions/* "$OUTPUT_DIR/data/millionaire/questions/" 2>/dev/null || true
fi

# ============================================================
# Summary
# ============================================================
echo ""
echo -e "${GREEN}=== Build complete! ===${NC}"
echo ""
echo -e "${GREEN}Output directory: ${OUTPUT_DIR}${NC}"
echo ""
echo "Directory structure:"
echo "  bins/"
echo "    llizardgui-host     - Main GUI application"
echo "    mediadash-client    - BLE media bridge daemon"
echo "    plugins/"
ls -1 "$OUTPUT_DIR/bins/plugins/" 2>/dev/null | sed 's/^/      /'
echo "  fonts/"
ls -1 "$OUTPUT_DIR/fonts/" 2>/dev/null | sed 's/^/      /' || echo "      (none)"
echo "  data/"
echo "      flashcards/questions/"
echo "      millionaire/questions/"
echo ""
echo -e "${YELLOW}To use in your image build:${NC}"
echo "  cp -r $OUTPUT_DIR/* /path/to/llizardOS/resources/"
