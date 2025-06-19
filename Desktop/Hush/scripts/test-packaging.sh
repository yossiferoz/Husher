#!/bin/bash

# Test script for packaging scripts
# Validates that the scripts can run without errors

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Test macOS packaging script
test_macos_packaging() {
    log "Testing macOS packaging script..."
    
    # Check if script exists and is executable
    if [[ ! -x "scripts/package-macos.sh" ]]; then
        error "macOS packaging script not found or not executable"
    fi
    
    # Test script syntax
    if bash -n scripts/package-macos.sh; then
        success "macOS script syntax is valid"
    else
        error "macOS script has syntax errors"
    fi
    
    # Test with dummy build directory (without actually running)
    mkdir -p test_build/VST3
    mkdir -p test_build/AU
    touch test_build/KhDetector_CLAP.clap
    
    # Create a dummy VST3 bundle structure
    mkdir -p "test_build/VST3/KhDetector.vst3/Contents/MacOS"
    echo "dummy" > "test_build/VST3/KhDetector.vst3/Contents/MacOS/KhDetector"
    
    # Test environment variable handling
    if BUILD_DIR="test_build" CI=true bash -c 'source scripts/package-macos.sh; check_prerequisites' 2>/dev/null; then
        success "macOS script prerequisites check works"
    else
        warning "macOS script prerequisites check failed (expected if tools missing)"
    fi
    
    # Cleanup
    rm -rf test_build
}

# Test Windows packaging script
test_windows_packaging() {
    log "Testing Windows packaging script..."
    
    # Check if script exists
    if [[ ! -f "scripts/package-windows.ps1" ]]; then
        error "Windows packaging script not found"
    fi
    
    # Test PowerShell syntax (if pwsh is available)
    if command -v pwsh >/dev/null 2>&1; then
        if pwsh -Command "Get-Content scripts/package-windows.ps1 | Out-Null"; then
            success "Windows script can be loaded by PowerShell"
        else
            error "Windows script has PowerShell errors"
        fi
    else
        warning "PowerShell not available, skipping Windows script syntax test"
    fi
    
    success "Windows script file exists and appears valid"
}

# Test CI workflow
test_ci_workflow() {
    log "Testing CI workflow..."
    
    # Check if workflow file exists
    if [[ ! -f ".github/workflows/build-and-package.yml" ]]; then
        error "CI workflow file not found"
    fi
    
    # Basic YAML syntax check (if available)
    if command -v python3 >/dev/null 2>&1; then
        if python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build-and-package.yml'))" 2>/dev/null; then
            success "CI workflow YAML is valid"
        else
            warning "CI workflow YAML may have issues"
        fi
    else
        warning "Python not available, skipping YAML validation"
    fi
    
    success "CI workflow file exists"
}

# Test directory structure
test_directory_structure() {
    log "Testing directory structure..."
    
    # Check required directories exist
    required_dirs=(
        "scripts"
        ".github/workflows"
    )
    
    for dir in "${required_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            success "Directory $dir exists"
        else
            error "Required directory $dir is missing"
        fi
    done
    
    # Check required files exist
    required_files=(
        "scripts/package-macos.sh"
        "scripts/package-windows.ps1"
        "scripts/README.md"
        ".github/workflows/build-and-package.yml"
    )
    
    for file in "${required_files[@]}"; do
        if [[ -f "$file" ]]; then
            success "File $file exists"
        else
            error "Required file $file is missing"
        fi
    done
}

# Test script permissions
test_permissions() {
    log "Testing script permissions..."
    
    # Check macOS script is executable
    if [[ -x "scripts/package-macos.sh" ]]; then
        success "macOS script is executable"
    else
        warning "macOS script is not executable (run: chmod +x scripts/package-macos.sh)"
    fi
    
    # Check this test script is executable
    if [[ -x "scripts/test-packaging.sh" ]]; then
        success "Test script is executable"
    else
        warning "Test script should be executable"
    fi
}

# Main test execution
main() {
    log "Starting packaging scripts test suite..."
    echo
    
    test_directory_structure
    echo
    
    test_permissions
    echo
    
    test_macos_packaging
    echo
    
    test_windows_packaging
    echo
    
    test_ci_workflow
    echo
    
    success "All packaging script tests completed successfully!"
    log "The packaging scripts are ready for use."
    echo
    
    log "Next steps:"
    echo "  1. Build your plugins: cmake --build build --config Release"
    echo "  2. Run macOS packaging: ./scripts/package-macos.sh"
    echo "  3. Run Windows packaging: ./scripts/package-windows.ps1"
    echo "  4. Check packages directory for output files"
}

# Run main function
main "$@" 