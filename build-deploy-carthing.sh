#!/bin/bash

# Build and deploy script for llizardgui-host to CarThing device
# CarThing IP: 172.16.42.2, user: root, password: llizardos
#
# Plugin Resources:
#   Source location: supporting_projects/salamanders/{plugin}/questions/
#   Build copies to: plugins/{plugin}/questions/
#   Deploy target:   /tmp/{plugin}/questions/ on CarThing

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

# Stop running host and plugins before deploying
echo -e "${YELLOW}Stopping llizardgui-host and plugins on CarThing...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "sv stop llizardGUI 2>/dev/null || true; killall llizardgui-host 2>/dev/null || true"

# Remount filesystem as read-write if needed
echo -e "${YELLOW}Ensuring filesystem is writable...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mount -o remount,rw / 2>/dev/null || true"

# Create plugins directory on CarThing if needed
echo -e "${YELLOW}Creating plugins directory on CarThing...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /usr/lib/llizard/plugins"

# Deploy main executable
echo -e "${YELLOW}Copying main executable...${NC}"
sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no llizardgui-host "$CARTHING_USER@$CARTHING_IP:/tmp/"

# Deploy all plugins
echo -e "${YELLOW}Copying plugins...${NC}"
for plugin in *.so; do
    if [ -f "$plugin" ]; then
        echo "  - Deploying $plugin"
        DEST_PATH="/usr/lib/llizard/plugins/$plugin"
        if ! sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$plugin" "$CARTHING_USER@$CARTHING_IP:$DEST_PATH" 2>/dev/null; then
            echo -e "${YELLOW}    File in use, killing process holding $plugin...${NC}"
            sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "fuser -k $DEST_PATH 2>/dev/null || true"
            sleep 0.5
            sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$plugin" "$CARTHING_USER@$CARTHING_IP:$DEST_PATH"
        fi
    fi
done

# Deploy flashcards questions folder
# Source: supporting_projects/salamanders/flashcards/questions/ (copied to plugins/ at build time)
FLASHCARDS_QUESTIONS="../plugins/flashcards/questions"
FLASHCARDS_QUESTIONS_SRC="../supporting_projects/salamanders/flashcards/questions"
if [ -d "$FLASHCARDS_QUESTIONS" ]; then
    echo -e "${YELLOW}Copying flashcards questions (from build output)...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/flashcards/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no "$FLASHCARDS_QUESTIONS"/* "$CARTHING_USER@$CARTHING_IP:/tmp/flashcards/questions/"
elif [ -d "$FLASHCARDS_QUESTIONS_SRC" ]; then
    echo -e "${YELLOW}Copying flashcards questions (from salamanders source)...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/flashcards/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no "$FLASHCARDS_QUESTIONS_SRC"/* "$CARTHING_USER@$CARTHING_IP:/tmp/flashcards/questions/"
fi

# Deploy millionaire questions folder
# Source: supporting_projects/salamanders/millionaire/questions/ (copied to plugins/ at build time)
MILLIONAIRE_QUESTIONS="../plugins/millionaire/questions"
MILLIONAIRE_QUESTIONS_SRC="../supporting_projects/salamanders/millionaire/questions"
if [ -d "$MILLIONAIRE_QUESTIONS" ]; then
    echo -e "${YELLOW}Copying millionaire questions (from build output)...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/millionaire/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no "$MILLIONAIRE_QUESTIONS"/* "$CARTHING_USER@$CARTHING_IP:/tmp/millionaire/questions/"
elif [ -d "$MILLIONAIRE_QUESTIONS_SRC" ]; then
    echo -e "${YELLOW}Copying millionaire questions (from salamanders source)...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mkdir -p /tmp/millionaire/questions"
    sshpass -p "$CARTHING_PASS" scp -r -o StrictHostKeyChecking=no "$MILLIONAIRE_QUESTIONS_SRC"/* "$CARTHING_USER@$CARTHING_IP:/tmp/millionaire/questions/"
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

# Remount filesystem as read-only
echo -e "${YELLOW}Remounting filesystem as read-only...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "mount -o remount,ro /"

# Restart llizardGUI service
echo -e "${YELLOW}Restarting llizardGUI service...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "sv start llizardGUI"

echo -e "${GREEN}=== Deployment complete! ===${NC}"