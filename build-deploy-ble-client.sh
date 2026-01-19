#!/bin/bash

# Build and deploy script for golang_ble_client to CarThing device
# CarThing IP: 172.16.42.2, user: root, password: nocturne

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building golang_ble_client for CarThing (ARM) ===${NC}"

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=""

if ! command -v go &> /dev/null; then
    MISSING_DEPS="$MISSING_DEPS golang-go"
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

# Navigate to golang_ble_client directory
cd "$(dirname "$0")/supporting_projects/golang_ble_client"

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

echo -e "${GREEN}=== Build complete! ===${NC}"
echo -e "${GREEN}=== Deploying to CarThing ===${NC}"

# CarThing connection details
CARTHING_IP="172.16.42.2"
CARTHING_USER="root"
CARTHING_PASS="nocturne"

# Deploy main executable
echo -e "${YELLOW}Copying mediadash-client to CarThing...${NC}"
sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no bin/mediadash-client "$CARTHING_USER@$CARTHING_IP:/tmp/"

# Set executable permissions
echo -e "${YELLOW}Setting executable permissions...${NC}"
sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "chmod +x /tmp/mediadash-client"

# Deploy config.json if it exists
if [ -f "config.json" ]; then
    echo -e "${YELLOW}Copying config.json...${NC}"
    sshpass -p "$CARTHING_PASS" scp -o StrictHostKeyChecking=no config.json "$CARTHING_USER@$CARTHING_IP:/tmp/"
fi

# Check and start Redis if needed
echo -e "${YELLOW}Checking Redis status...${NC}"
REDIS_STATUS=$(sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "sv status redis 2>&1" || true)
if [[ $REDIS_STATUS == *"run:"* ]]; then
    echo -e "${GREEN}Redis is already running${NC}"
else
    echo -e "${YELLOW}Starting Redis...${NC}"
    sshpass -p "$CARTHING_PASS" ssh -o StrictHostKeyChecking=no "$CARTHING_USER@$CARTHING_IP" "sv start redis"
fi

echo -e "${GREEN}=== Deployment complete! ===${NC}"
echo -e "${GREEN}To run on CarThing:${NC}"
echo "  ssh root@$CARTHING_IP"
echo "  cd /tmp && ./mediadash-client"
