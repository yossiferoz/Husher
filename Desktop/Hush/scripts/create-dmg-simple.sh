#!/bin/bash

# Simple KhDetector DMG Creation Script
# Creates a basic DMG installer for macOS without complex styling

set -euo pipefail

# Configuration
PRODUCT_NAME="KhDetector"
PRODUCT_VERSION="${PRODUCT_VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build_clap}"
DMG_DIR="${DMG_DIR:-dmg_build}"
OUTPUT_DIR="${OUTPUT_DIR:-packages}"

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

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log "Checking prerequisites..."
    
    # Check if build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        error "Build directory '$BUILD_DIR' not found. Run build first."
    fi
    
    # Check for built CLAP plugin
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    if [[ ! -d "$clap_path" ]]; then
        error "CLAP plugin not found at $clap_path"
    fi
    
    success "Prerequisites check passed"
}

# Create DMG structure
create_dmg_structure() {
    log "Creating DMG structure..."
    
    # Clean and create directories
    rm -rf "$DMG_DIR"
    mkdir -p "$DMG_DIR"
    mkdir -p "$OUTPUT_DIR"
    
    # Create the DMG content directory
    mkdir -p "$DMG_DIR/KhDetector"
    
    success "DMG structure created"
}

# Copy plugin files and create installer
copy_and_create_installer() {
    log "Copying plugins and creating installer..."
    
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    # Copy CLAP plugin
    cp -R "$clap_path" "$DMG_DIR/KhDetector/"
    
    # Create installer script
    cat > "$DMG_DIR/KhDetector/Install KhDetector.command" << 'EOF'
#!/bin/bash

# KhDetector Automatic Installer
set -euo pipefail

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[KhDetector Installer]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CLAP_PLUGIN="$SCRIPT_DIR/KhDetector_CLAP.clap"

log "KhDetector Plugin Installer"
log "=========================="
echo

if [[ ! -d "$CLAP_PLUGIN" ]]; then
    echo "Error: KhDetector_CLAP.clap not found!"
    exit 1
fi

USER_DEST_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"

echo "Installing KhDetector to: $USER_DEST_DIR"
echo

mkdir -p "$USER_DEST_DIR"
cp -R "$CLAP_PLUGIN" "$USER_DEST_DIR/"
chmod -R 755 "$USER_DEST_DIR/KhDetector_CLAP.clap"

success "Plugin installed successfully!"
echo
log "Next steps:"
log "1. Restart Ableton Live"
log "2. Rescan plugins if necessary"
log "3. Look for 'KhDetector_CLAP' in your plugin list"
echo
log "The plugin provides real-time kick drum detection with MIDI output."
echo

read -p "Press Enter to close this installer..."
EOF

    chmod +x "$DMG_DIR/KhDetector/Install KhDetector.command"
    
    # Create README
    cat > "$DMG_DIR/KhDetector/README.txt" << EOF
KhDetector v${PRODUCT_VERSION} - Kick Drum Detection Plugin
========================================================

INSTALLATION:
1. Double-click "Install KhDetector.command" for automatic installation
2. Or manually copy KhDetector_CLAP.clap to ~/Library/Audio/Plug-Ins/CLAP/

SUPPORTED DAWs:
- Ableton Live 11.3+ (CLAP support)
- Reaper (full CLAP support)  
- Bitwig Studio (native CLAP support)
- FL Studio (newer versions)

USAGE:
1. Load KhDetector on a drum track
2. The plugin will output MIDI Note 45 (A2) when kicks are detected
3. Route the MIDI output to trigger samples or instruments

FEATURES:
- Real-time kick drum detection using AI
- Low-latency processing (<1ms)
- SIMD optimized for modern CPUs
- Lock-free real-time safe processing

For support: https://github.com/khdetector
EOF
    
    success "Installer and documentation created"
}

# Build the DMG
build_dmg() {
    log "Building DMG..."
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    # Create DMG directly from source folder
    hdiutil create -srcfolder "$DMG_DIR" \
                   -volname "KhDetector v${PRODUCT_VERSION}" \
                   -fs HFS+ \
                   -format UDZO \
                   -imagekey zlib-level=9 \
                   "$dmg_path"
    
    success "DMG created: $dmg_path"
    
    # Show file size
    local file_size=$(du -h "$dmg_path" | cut -f1)
    log "DMG size: $file_size"
}

# Create checksums
create_checksums() {
    log "Creating checksums..."
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    # SHA256
    shasum -a 256 "$dmg_path" > "$dmg_path.sha256"
    
    # MD5
    md5 "$dmg_path" > "$dmg_path.md5"
    
    success "Checksums created"
}

# Cleanup
cleanup() {
    log "Cleaning up temporary files..."
    rm -rf "$DMG_DIR"
    success "Cleanup completed"
}

# Main execution
main() {
    log "Starting simple DMG creation for $PRODUCT_NAME v$PRODUCT_VERSION"
    
    check_prerequisites
    create_dmg_structure
    copy_and_create_installer
    build_dmg
    create_checksums
    cleanup
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    success "DMG created successfully!"
    log "DMG location: $dmg_path"
    log "DMG size: $(du -h "$dmg_path" | cut -f1)"
    echo
    log "The DMG contains:"
    log "  • KhDetector_CLAP.clap (plugin file)"
    log "  • Install KhDetector.command (automatic installer)"
    log "  • README.txt (documentation)"
    echo
    log "Users can double-click the DMG and run the installer for easy installation."
}

# Handle script termination
trap cleanup EXIT

# Run main function
main "$@" 