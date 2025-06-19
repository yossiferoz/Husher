#!/bin/bash

# KhDetector Build Script
# Cross-platform VST3 Plugin Builder

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}KhDetector VST3 Plugin Build Script${NC}"
echo -e "${BLUE}===================================${NC}"

# Default values
BUILD_TYPE="Debug"
CLEAN_BUILD=false
INSTALL_PLUGIN=false
RUN_TESTS=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL_PLUGIN=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -r, --release     Build in Release mode (default: Debug)"
            echo "  -d, --debug       Build in Debug mode"
            echo "  -c, --clean       Clean build directory before building"
            echo "  -i, --install     Install plugin to system directory after building"
            echo "  -t, --test        Run unit tests after building"
            echo "  -h, --help        Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${YELLOW}Build Configuration:${NC}"
echo -e "  Build Type: ${BUILD_TYPE}"
echo -e "  Clean Build: ${CLEAN_BUILD}"
echo -e "  Install Plugin: ${INSTALL_PLUGIN}"
echo -e "  Run Tests: ${RUN_TESTS}"
echo ""

# Check if we're on macOS or Linux
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    CMAKE_ARGS="-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    CMAKE_ARGS=""
else
    echo -e "${RED}Unsupported platform: $OSTYPE${NC}"
    echo -e "${YELLOW}This script is for macOS and Linux. For Windows, use build.bat${NC}"
    exit 1
fi

echo -e "${BLUE}Building for: ${PLATFORM}${NC}"

# Check for required tools
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: CMake is not installed${NC}"
    echo -e "${YELLOW}Please install CMake 3.20 or later${NC}"
    exit 1
fi

if ! command -v git &> /dev/null; then
    echo -e "${RED}Error: Git is not installed${NC}"
    exit 1
fi

# Initialize submodules if needed
if [ ! -f "external/vst3sdk/CMakeLists.txt" ]; then
    echo -e "${YELLOW}Initializing VST3 SDK submodule...${NC}"
    git submodule update --init --recursive
fi

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    ${CMAKE_ARGS}

# Build
echo -e "${YELLOW}Building KhDetector...${NC}"
cmake --build build --config ${BUILD_TYPE} --parallel

# Check if build was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build completed successfully!${NC}"
    
    # Find the built plugin
    if [[ "$PLATFORM" == "macOS" ]]; then
        PLUGIN_PATH="build/KhDetector.vst3"
        INSTALL_PATH="$HOME/Library/Audio/Plug-Ins/VST3/KhDetector.vst3"
    else
        PLUGIN_PATH="build/KhDetector.so"
        INSTALL_PATH="$HOME/.vst3/KhDetector.vst3"
    fi
    
    if [ -e "$PLUGIN_PATH" ]; then
        echo -e "${GREEN}Plugin built at: ${PLUGIN_PATH}${NC}"
        
        # Install if requested
        if [ "$INSTALL_PLUGIN" = true ]; then
            echo -e "${YELLOW}Installing plugin...${NC}"
            if [[ "$PLATFORM" == "macOS" ]]; then
                cp -R "$PLUGIN_PATH" "$INSTALL_PATH"
            else
                mkdir -p "$HOME/.vst3"
                cp "$PLUGIN_PATH" "$INSTALL_PATH"
            fi
            echo -e "${GREEN}✓ Plugin installed to: ${INSTALL_PATH}${NC}"
        fi
        
        # Run tests if requested
        if [ "$RUN_TESTS" = true ]; then
            echo -e "${YELLOW}Running unit tests...${NC}"
            if command -v ctest &> /dev/null; then
                cd build && ctest --verbose --output-on-failure
                if [ $? -eq 0 ]; then
                    echo -e "${GREEN}✓ All tests passed!${NC}"
                else
                    echo -e "${RED}✗ Some tests failed!${NC}"
                fi
                cd ..
            else
                echo -e "${YELLOW}Warning: CTest not found, running tests directly...${NC}"
                if [ -f "build/KhDetectorTests" ]; then
                    ./build/KhDetectorTests
                elif [ -f "build/${BUILD_TYPE}/KhDetectorTests.exe" ]; then
                    ./build/${BUILD_TYPE}/KhDetectorTests.exe
                else
                    echo -e "${RED}✗ Test executable not found!${NC}"
                fi
            fi
        fi
    else
        echo -e "${YELLOW}Warning: Plugin file not found at expected location${NC}"
    fi
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Build complete!${NC}" 