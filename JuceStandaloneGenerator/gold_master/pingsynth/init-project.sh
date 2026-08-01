#!/usr/bin/env bash
set -e

CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

git init

echo -e "${CYAN}abacdsp setup:${NC}"
echo "1) Add as git submodule"
echo "2) Create softlink to local installation"
echo -ne "${YELLOW}Choice [1/2]: ${NC}"
read choice

if [ "$choice" = "2" ]; then
    echo -ne "${YELLOW}Path to abacdsp library [~/projects/abacdsp]: ${NC}"
    read abacdsp_path
    abacdsp_path=${abacdsp_path:-~/projects/abacdsp}
    abacdsp_path=$(eval echo "$abacdsp_path")

    if [ ! -d "$abacdsp_path" ]; then
        echo -e "${RED}Error: Directory $abacdsp_path does not exist${NC}"
        exit 1
    fi

    if [ ! -d "$abacdsp_path/.git" ]; then
        echo -e "${YELLOW}Warning: $abacdsp_path is not a git repository${NC}"
    else
        echo -e "${CYAN}Git info:${NC}"
        git --no-pager -C "$abacdsp_path" log -1 --oneline --no-decorate
        branch=$(git -C "$abacdsp_path" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        dirty=$(git -C "$abacdsp_path" diff --quiet 2>/dev/null || echo " (modified)")
        echo "Branch: ${branch}${dirty}"
    fi

    echo ""
    echo -ne "${YELLOW}Proceed with this configuration? [y/N]: ${NC}"
    read confirm

    if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
        echo -e "${RED}Aborted${NC}"
        exit 1
    fi
fi

# Add all submodules
git submodule add https://github.com/juce-framework/JUCE.git 3rdparty/JUCE

git submodule add https://github.com/google/googletest.git 3rdparty/googletest

if [ "$choice" = "2" ]; then
    mkdir -p 3rdparty
    ln -s "$abacdsp_path" 3rdparty/abacdsp
    echo -e "${GREEN}Softlink created${NC}"
else
    git submodule add https://github.com/scjurgen64/abacdsp.git 3rdparty/abacdsp
fi
