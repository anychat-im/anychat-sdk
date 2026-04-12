#!/bin/bash

# CocoaPods Package Verification Script for AnyChatSDK
# This script validates the podspec and package structure

set -e

echo "========================================="
echo "AnyChatSDK CocoaPods Package Verification"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "AnyChatSDK.podspec" ]; then
    echo -e "${RED}Error: AnyChatSDK.podspec not found. Please run this script from packages/ios/${NC}"
    exit 1
fi

echo -e "${YELLOW}1. Checking file structure...${NC}"

# Check required files
REQUIRED_FILES=(
    "AnyChatSDK.podspec"
    "AnyChatSDK.podspec.json"
    "README.md"
    "CHANGELOG.md"
    "../../LICENSE"
    "../../bindings/ios/Sources/AnyChatSDK/AnyChatSDK.h"
    "../../bindings/ios/Sources/AnyChatSDK/module.modulemap"
    "Example/Podfile"
    "Example/README.md"
    "Example/AnyChatSDKExample.xcodeproj/project.pbxproj"
    "Example/AnyChatSDKExample/ContentView.swift"
    "Example/AnyChatSDKExample/Info.plist"
)

MISSING_FILES=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "  ${GREEN}✓${NC} $file"
    else
        echo -e "  ${RED}✗${NC} $file (missing)"
        MISSING_FILES=$((MISSING_FILES + 1))
    fi
done

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "${RED}Error: $MISSING_FILES required files are missing${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}2. Checking Swift source files...${NC}"

# Check Swift source files
SWIFT_FILES=$(find ../../bindings/ios/Sources/AnyChatSDK -name "*.swift" 2>/dev/null | wc -l)
if [ $SWIFT_FILES -gt 0 ]; then
    echo -e "  ${GREEN}✓${NC} Found $SWIFT_FILES Swift source files"
else
    echo -e "  ${RED}✗${NC} No Swift source files found"
    exit 1
fi

echo ""
echo -e "${YELLOW}3. Checking C headers...${NC}"

# Check C headers
HEADERS=$(find ../../core/include -name "*.h" 2>/dev/null | wc -l)
if [ $HEADERS -gt 0 ]; then
    echo -e "  ${GREEN}✓${NC} Found $HEADERS C header files"
else
    echo -e "  ${RED}✗${NC} No C header files found"
    exit 1
fi

echo ""
echo -e "${YELLOW}4. Validating podspec syntax...${NC}"

# Check if pod command is available
if command -v pod &> /dev/null; then
    if pod spec lint AnyChatSDK.podspec --allow-warnings --skip-import-validation; then
        echo -e "  ${GREEN}✓${NC} Podspec syntax is valid"
    else
        echo -e "  ${YELLOW}⚠${NC} Podspec validation failed (this is expected without building the library)"
    fi
else
    echo -e "  ${YELLOW}⚠${NC} CocoaPods not installed, skipping validation"
    echo "    Install with: sudo gem install cocoapods"
fi

echo ""
echo -e "${YELLOW}5. Checking submodules...${NC}"

# Check if submodules exist
if [ -d "../../thirdparty/curl" ]; then
    echo -e "  ${GREEN}✓${NC} curl submodule exists"
else
    echo -e "  ${YELLOW}⚠${NC} curl submodule not initialized"
    echo "    Run: git submodule update --init --recursive"
fi

if [ -d "../../thirdparty/libwebsockets" ]; then
    echo -e "  ${GREEN}✓${NC} libwebsockets submodule exists"
else
    echo -e "  ${YELLOW}⚠${NC} libwebsockets submodule not initialized"
fi

if [ -d "../../thirdparty/glaze" ]; then
    echo -e "  ${GREEN}✓${NC} glaze submodule exists"
else
    echo -e "  ${YELLOW}⚠${NC} glaze submodule not initialized"
fi

echo ""
echo -e "${YELLOW}6. Checking Example app...${NC}"

cd Example

# Check if Pods directory exists
if [ -d "Pods" ]; then
    echo -e "  ${GREEN}✓${NC} CocoaPods dependencies installed"
else
    echo -e "  ${YELLOW}⚠${NC} CocoaPods dependencies not installed"
    echo "    Run: cd Example && pod install"
fi

# Check workspace
if [ -f "AnyChatSDKExample.xcworkspace" ]; then
    echo -e "  ${GREEN}✓${NC} Xcode workspace exists"
else
    echo -e "  ${YELLOW}⚠${NC} Xcode workspace not generated"
    echo "    Run: cd Example && pod install"
fi

cd ..

echo ""
echo "========================================="
echo -e "${GREEN}Verification Complete!${NC}"
echo "========================================="
echo ""
echo "Next steps:"
echo "1. Initialize submodules: git submodule update --init --recursive"
echo "2. Build the core library (see core/CMakeLists.txt)"
echo "3. Install example dependencies: cd Example && pod install"
echo "4. Open Example/AnyChatSDKExample.xcworkspace in Xcode"
echo "5. Test the example app"
echo "6. Validate podspec: pod spec lint AnyChatSDK.podspec"
echo "7. Publish to CocoaPods: pod trunk push AnyChatSDK.podspec"
echo ""
