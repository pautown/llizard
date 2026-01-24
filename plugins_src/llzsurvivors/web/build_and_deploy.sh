#!/bin/bash
# build_and_deploy.sh - Build LLZ Survivors for web and deploy to townhaus
#
# Usage:
#   ./build_and_deploy.sh          # Build and deploy
#   ./build_and_deploy.sh --clean  # Clean build first
#   ./build_and_deploy.sh --skip-build  # Just deploy existing build

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
TOWNHAUS_DIR="$PROJECT_ROOT/supporting_projects/townhaus"
DEPLOY_DIR="$TOWNHAUS_DIR/pages/games/survivors"
BUILD_DIR="$SCRIPT_DIR/build"

# raylib path (can be overridden with RAYLIB_PATH env var)
RAYLIB_PATH="${RAYLIB_PATH:-$HOME/raylib}"

echo -e "${CYAN}╔════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║   LLZ Survivors Web Build & Deploy Script  ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════╝${NC}"
echo ""

# Parse arguments
SKIP_BUILD=false
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        --skip-build)
            SKIP_BUILD=true
            ;;
        --clean)
            CLEAN_BUILD=true
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --clean       Clean build directory before building"
            echo "  --skip-build  Skip building, just deploy existing build"
            echo "  --help        Show this help"
            exit 0
            ;;
    esac
done

# Check for Emscripten
check_emscripten() {
    if ! command -v emcc &> /dev/null; then
        echo -e "${RED}Error: Emscripten (emcc) not found in PATH${NC}"
        echo ""
        echo "To install Emscripten:"
        echo "  git clone https://github.com/emscripten-core/emsdk.git"
        echo "  cd emsdk"
        echo "  ./emsdk install latest"
        echo "  ./emsdk activate latest"
        echo "  source ./emsdk_env.sh"
        echo ""
        echo "Or add to your shell profile:"
        echo "  source ~/emsdk/emsdk_env.sh"
        return 1
    fi
    echo -e "${GREEN}✓ Emscripten found: $(emcc --version | head -1)${NC}"
    return 0
}

# Check for raylib
check_raylib() {
    # Web builds create libraylib.web.a, desktop creates libraylib.a
    if [ ! -f "$RAYLIB_PATH/src/libraylib.a" ] && [ ! -f "$RAYLIB_PATH/src/libraylib.web.a" ]; then
        echo -e "${YELLOW}Warning: raylib not found at $RAYLIB_PATH${NC}"
        echo ""
        echo "Building raylib for web..."

        if [ ! -d "$RAYLIB_PATH" ]; then
            echo "Cloning raylib..."
            git clone https://github.com/raysan5/raylib.git "$RAYLIB_PATH"
        fi

        cd "$RAYLIB_PATH/src"
        make PLATFORM=PLATFORM_WEB -B
        cd "$SCRIPT_DIR"

        if [ ! -f "$RAYLIB_PATH/src/libraylib.a" ] && [ ! -f "$RAYLIB_PATH/src/libraylib.web.a" ]; then
            echo -e "${RED}Error: Failed to build raylib${NC}"
            return 1
        fi
    fi
    # Create symlink if web version exists but not standard name
    if [ -f "$RAYLIB_PATH/src/libraylib.web.a" ] && [ ! -f "$RAYLIB_PATH/src/libraylib.a" ]; then
        ln -sf "$RAYLIB_PATH/src/libraylib.web.a" "$RAYLIB_PATH/src/libraylib.a"
    fi
    echo -e "${GREEN}✓ raylib found at $RAYLIB_PATH${NC}"
    return 0
}

# Build the game
build_game() {
    echo ""
    echo -e "${CYAN}Building LLZ Survivors for web...${NC}"

    cd "$SCRIPT_DIR"

    if [ "$CLEAN_BUILD" = true ]; then
        echo "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"

    # Compiler flags
    CFLAGS="-Wall -Os -DPLATFORM_WEB -DNDEBUG"
    CFLAGS="$CFLAGS -I$SCRIPT_DIR -I$GAME_DIR -I$RAYLIB_PATH/src"

    # Emscripten flags
    EMFLAGS="-s USE_GLFW=3"
    EMFLAGS="$EMFLAGS -s TOTAL_MEMORY=67108864"
    EMFLAGS="$EMFLAGS -s ALLOW_MEMORY_GROWTH=1"
    EMFLAGS="$EMFLAGS -s ASYNCIFY"
    EMFLAGS="$EMFLAGS -s FORCE_FILESYSTEM=1"
    EMFLAGS="$EMFLAGS -s EXPORTED_FUNCTIONS=[_main,_WebInit]"
    EMFLAGS="$EMFLAGS -s EXPORTED_RUNTIME_METHODS=[ccall,cwrap]"
    EMFLAGS="$EMFLAGS --shell-file $SCRIPT_DIR/shell.html"

    # Source files
    SOURCES="$SCRIPT_DIR/main_web.c $GAME_DIR/llzsurvivors_game.c"

    # Build
    echo "Compiling..."
    emcc $CFLAGS $EMFLAGS -o "$BUILD_DIR/index.html" $SOURCES "$RAYLIB_PATH/src/libraylib.a" -lm

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful!${NC}"
        echo "  Output: $BUILD_DIR/"
        ls -lh "$BUILD_DIR"/*.{html,js,wasm} 2>/dev/null || true
    else
        echo -e "${RED}✗ Build failed${NC}"
        return 1
    fi
}

# Deploy to townhaus
deploy_to_townhaus() {
    echo ""
    echo -e "${CYAN}Deploying to townhaus...${NC}"

    # Check if build exists
    if [ ! -f "$BUILD_DIR/index.html" ]; then
        echo -e "${RED}Error: Build not found at $BUILD_DIR/index.html${NC}"
        echo "Run without --skip-build first"
        return 1
    fi

    # Create deploy directory
    mkdir -p "$DEPLOY_DIR"

    # Copy files
    echo "Copying files to $DEPLOY_DIR..."
    cp "$BUILD_DIR/index.html" "$DEPLOY_DIR/"
    cp "$BUILD_DIR/index.js" "$DEPLOY_DIR/" 2>/dev/null || true
    cp "$BUILD_DIR/index.wasm" "$DEPLOY_DIR/" 2>/dev/null || true

    # Also copy any .data files (for preloaded assets)
    cp "$BUILD_DIR"/*.data "$DEPLOY_DIR/" 2>/dev/null || true

    echo -e "${GREEN}✓ Deployed to $DEPLOY_DIR${NC}"
    ls -lh "$DEPLOY_DIR"
}

# Main
main() {
    if [ "$SKIP_BUILD" = false ]; then
        if ! check_emscripten; then
            echo ""
            echo -e "${YELLOW}Skipping build - Emscripten not available${NC}"
            echo "You can still deploy an existing build with --skip-build"
            exit 1
        fi

        if ! check_raylib; then
            exit 1
        fi

        if ! build_game; then
            exit 1
        fi
    fi

    deploy_to_townhaus

    echo ""
    echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║              Deploy Complete!              ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
    echo ""
    echo "To test locally:"
    echo "  cd $DEPLOY_DIR"
    echo "  python3 -m http.server 8000"
    echo "  open http://localhost:8000/"
    echo ""
    echo "To publish:"
    echo "  cd $TOWNHAUS_DIR"
    echo "  git add pages/games/survivors/"
    echo "  git commit -m 'Add LLZ Survivors web game'"
    echo "  git push"
}

main "$@"
