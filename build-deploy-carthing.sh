#!/bin/bash

# Build and deploy script for llizardgui-host to CarThing device
# CarThing IP: 172.16.42.2, user: root, password: llizardos

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building llizardgui-host for CarThing (ARM/DRM) ===${NC}"

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=""

if ! command -v cmake &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS cmake"
fi

if ! command -v arm-linux-gnueabihf-gcc &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS gcc-arm-linux-gnueabihf"
fi

if ! command -v sshpass &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS sshpass"
fi

if [ ! -z "$MISSING_DEPS" ]; then
    echo -e "${RED}Missing dependencies:${NC}$MISSING_DEPS"
    echo -e "${YELLOW}Install with: sudo apt install$MISSING_DEPS${NC}"
    exit 1
fi

echo -e "${GREEN}All dependencies found!${NC}"

# Navigate to project root
cd "$(dirname "$0")"

# Create and enter build directory
echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build-armv7-drm
cd build-armv7-drm

# Configure with ARM toolchain and DRM platform
echo -e "${YELLOW}Configuring CMake for ARM/DRM...${NC}"
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armv7.cmake -DPLATFORM=DRM ..

# Build everything
echo -e "${YELLOW}Building project...${NC}"
make -j$(nproc)

echo -e "${GREEN}=== Build complete! ===${NC}"
echo -e "${GREEN}=== Deploying to CarThing ===${NC}"

# CarThing connection details
CARTHING_IP="172.16.42.2"
CARTHING_USER="root"
CARTHING_PASS="llizardos"

# Create plugins directory on CarThing if needed
echo -e "${YELLOW}Creating plugins directory on CarThing...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/plugins"

# Deploy main executable
echo -e "${YELLOW}Copying main executable...${NC}"
sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no llizardgui-host "$CARTHING_USER@$CARTHING_IP:/tmp/"

# Deploy all plugins
echo -e "${YELLOW}Copying plugins...${NC}"
for plugin in *.so; do
    if [ -f "$plugin" ]; then
        echo "  - Deploying $plugin"
        sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$plugin" "$CARTHING_USER@$CARTHING_IP:/tmp/plugins/"
    fi
done

# Deploy flashcards questions folder
if [ -d "../plugins/flashcards/questions" ]; then
    echo -e "${YELLOW}Copying flashcards questions...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/flashcards/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no ../plugins/flashcards/questions/* "$CARTHING_USER@$CARTHING_IP:/tmp/flashcards/questions/"
fi

# Deploy millionaire questions folder
if [ -d "../plugins/millionaire/questions" ]; then
    echo -e "${YELLOW}Copying millionaire questions...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/millionaire/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no ../plugins/millionaire/questions/* "$CARTHING_USER@$CARTHING_IP:/tmp/millionaire/questions/"
fi

# Deploy fonts (if any exist locally that aren't on the device)
if [ -d "../fonts" ] && [ "$(ls -A ../fonts/*.ttf 2>/dev/null)" ]; then
    echo -e "${YELLOW}Copying fonts...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/fonts"
    for font in ../fonts/*.ttf; do
        if [ -f "$font" ]; then
            fontname=$(basename "$font")
            echo "  - Deploying $fontname"
            sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$font" "$CARTHING_USER@$CARTHING_IP:/tmp/fonts/"
        fi
    done
fi

echo -e "${GREEN}=== Deployment complete! ===${NC}"
echo -e "${GREEN}To run on CarThing:${NC}"
echo "  ssh root@$CARTHING_IP"
echo "  cd /tmp && ./llizardgui-host"