#!/bin/bash

# Copy fonts from CarThing device to local fonts directory
# CarThing IP: 172.16.42.2, user: root, password: llizardos

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

CARTHING_IP="172.16.42.2"
CARTHING_USER="root"
CARTHING_PASS="llizardos"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FONTS_DIR="$PROJECT_DIR/fonts"

echo -e "${GREEN}=== Copying fonts from CarThing ===${NC}"

# Check for sshpass
if ! command -v sshpass &> /dev/null; then
    echo -e "${RED}sshpass is required. Install with: sudo apt install sshpass${NC}"
    exit 1
fi

# Ensure fonts directory exists
mkdir -p "$FONTS_DIR"

echo -e "${YELLOW}Connecting to CarThing at $CARTHING_IP...${NC}"

# Check connection
if ! sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$CARTHING_USER@$CARTHING_IP" "echo connected" &>/dev/null; then
    echo -e "${RED}Cannot connect to CarThing. Make sure it's connected via USB.${NC}"
    exit 1
fi

echo -e "${GREEN}Connected!${NC}"

# List available fonts on CarThing
echo -e "${YELLOW}Available fonts on CarThing:${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "ls -la /var/local/fonts/" 2>/dev/null || echo "No fonts directory found"

# Copy fonts
echo -e "${YELLOW}Copying fonts...${NC}"

# Copy ZegoeUI font
if sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP:/var/local/fonts/ZegoeUI-U.ttf" "$FONTS_DIR/" 2>/dev/null; then
    echo -e "${GREEN}  - Copied ZegoeUI-U.ttf${NC}"
else
    echo -e "${YELLOW}  - ZegoeUI-U.ttf not found or failed to copy${NC}"
fi

# Copy any other TTF fonts found
for font in $(sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "ls /var/local/fonts/*.ttf 2>/dev/null" 2>/dev/null || true); do
    fontname=$(basename "$font")
    if [ "$fontname" != "ZegoeUI-U.ttf" ]; then
        if sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP:$font" "$FONTS_DIR/" 2>/dev/null; then
            echo -e "${GREEN}  - Copied $fontname${NC}"
        fi
    fi
done

echo ""
echo -e "${GREEN}=== Fonts copied to $FONTS_DIR ===${NC}"
echo -e "${YELLOW}Contents:${NC}"
ls -la "$FONTS_DIR"/*.ttf 2>/dev/null || echo "No TTF files found"
