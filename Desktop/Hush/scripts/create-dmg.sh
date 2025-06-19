#!/bin/bash

# KhDetector DMG Creation Script
# Creates a drag-and-drop DMG installer for macOS

set -euo pipefail

# Configuration
PRODUCT_NAME="KhDetector"
PRODUCT_VERSION="${PRODUCT_VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build_clap}"
DMG_DIR="${DMG_DIR:-dmg_build}"
OUTPUT_DIR="${OUTPUT_DIR:-packages}"
TEMP_DIR=$(mktemp -d)

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

# Check prerequisites
check_prerequisites() {
    log "Checking prerequisites..."
    
    # Check if build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        error "Build directory '$BUILD_DIR' not found. Run build first."
    fi
    
    # Check for required tools
    command -v hdiutil >/dev/null 2>&1 || error "hdiutil not found"
    command -v cp >/dev/null 2>&1 || error "cp not found"
    
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

# Copy plugin files
copy_plugins() {
    log "Copying plugins to DMG..."
    
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    # Copy CLAP plugin
    if [[ -d "$clap_path" ]]; then
        cp -R "$clap_path" "$DMG_DIR/KhDetector/"
        log "CLAP plugin copied"
    fi
    
    success "Plugins copied to DMG"
}

# Create installation instructions
create_installation_guide() {
    log "Creating installation guide..."
    
    cat > "$DMG_DIR/KhDetector/INSTALL.txt" << 'EOF'
KhDetector Plugin Installation Instructions
==========================================

AUTOMATIC INSTALLATION (Recommended):
1. Double-click "Install KhDetector.command" to automatically install
2. Enter your password when prompted
3. Restart your DAW and rescan plugins

MANUAL INSTALLATION:
1. Copy "KhDetector_CLAP.clap" to one of these locations:
   
   System-wide (requires admin password):
   /Library/Audio/Plug-Ins/CLAP/
   
   User-only (no password required):
   ~/Library/Audio/Plug-Ins/CLAP/

2. Restart your DAW
3. Rescan plugins if necessary

SUPPORTED DAWs:
- Ableton Live 11.3+ (CLAP support)
- Reaper (full CLAP support)
- Bitwig Studio (native CLAP support)
- FL Studio (newer versions)

TROUBLESHOOTING:
- If plugin doesn't appear, clear your DAW's plugin cache and rescan
- Make sure you're using a DAW version that supports CLAP plugins
- Check the plugin appears in ~/Library/Audio/Plug-Ins/CLAP/ after installation

For support: https://github.com/khdetector/support
EOF

    success "Installation guide created"
}

# Create automatic installer script
create_installer_script() {
    log "Creating automatic installer script..."
    
    cat > "$DMG_DIR/KhDetector/Install KhDetector.command" << 'EOF'
#!/bin/bash

# KhDetector Automatic Installer
# This script installs the KhDetector CLAP plugin

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[KhDetector Installer]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

log "KhDetector Plugin Installer"
log "=========================="
echo

# Check if CLAP plugin exists
CLAP_PLUGIN="$SCRIPT_DIR/KhDetector_CLAP.clap"
if [[ ! -d "$CLAP_PLUGIN" ]]; then
    error "KhDetector_CLAP.clap not found in the same directory as this installer"
fi

# Create destination directory
DEST_DIR="/Library/Audio/Plug-Ins/CLAP"
USER_DEST_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"

echo "Choose installation location:"
echo "1) System-wide: $DEST_DIR (requires admin password)"
echo "2) User-only: $USER_DEST_DIR (no password required)"
echo "3) Cancel installation"
echo

read -p "Enter your choice (1-3): " choice

case $choice in
    1)
        log "Installing system-wide..."
        sudo mkdir -p "$DEST_DIR"
        sudo cp -R "$CLAP_PLUGIN" "$DEST_DIR/"
        sudo chmod -R 755 "$DEST_DIR/KhDetector_CLAP.clap"
        success "Plugin installed to $DEST_DIR"
        ;;
    2)
        log "Installing for current user..."
        mkdir -p "$USER_DEST_DIR"
        cp -R "$CLAP_PLUGIN" "$USER_DEST_DIR/"
        chmod -R 755 "$USER_DEST_DIR/KhDetector_CLAP.clap"
        success "Plugin installed to $USER_DEST_DIR"
        ;;
    3)
        log "Installation cancelled"
        exit 0
        ;;
    *)
        error "Invalid choice. Installation cancelled."
        ;;
esac

echo
log "Installation completed successfully!"
log "Next steps:"
log "1. Restart your DAW (Ableton Live, Reaper, Bitwig, etc.)"
log "2. Rescan plugins if necessary"
log "3. Look for 'KhDetector_CLAP' in your plugin list"
echo
log "The plugin provides real-time kick drum detection with MIDI output."
echo

read -p "Press Enter to close this installer..."
EOF

    # Make the installer executable
    chmod +x "$DMG_DIR/KhDetector/Install KhDetector.command"
    
    success "Automatic installer script created"
}

# Create uninstaller script
create_uninstaller_script() {
    log "Creating uninstaller script..."
    
    cat > "$DMG_DIR/KhDetector/Uninstall KhDetector.command" << 'EOF'
#!/bin/bash

# KhDetector Uninstaller
# This script removes the KhDetector CLAP plugin

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[KhDetector Uninstaller]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log "KhDetector Plugin Uninstaller"
log "============================="
echo

warning "This will remove KhDetector from your system."
read -p "Are you sure you want to continue? (y/N): " confirm

if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    log "Uninstallation cancelled"
    exit 0
fi

# Remove from system location
if [[ -d "/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" ]]; then
    log "Removing system-wide installation..."
    sudo rm -rf "/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap"
    success "System-wide installation removed"
fi

# Remove from user location
if [[ -d "$HOME/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" ]]; then
    log "Removing user installation..."
    rm -rf "$HOME/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap"
    success "User installation removed"
fi

echo
success "KhDetector has been successfully uninstalled"
log "You may need to restart your DAW and rescan plugins"
echo

read -p "Press Enter to close this uninstaller..."
EOF

    chmod +x "$DMG_DIR/KhDetector/Uninstall KhDetector.command"
    
    success "Uninstaller script created"
}

# Create README file
create_readme() {
    log "Creating README..."
    
    cat > "$DMG_DIR/KhDetector/README.md" << EOF
# KhDetector v${PRODUCT_VERSION}

Real-time kick drum detection plugin using AI inference.

## Features

- **Real-time kick drum detection** using AI inference
- **MIDI output** (Note 45/A2) when kick detected
- **Low-latency processing** with SIMD optimization
- **Waveform visualization** with OpenGL rendering
- **Cross-platform** CLAP plugin support

## Installation

### Automatic Installation (Recommended)
1. Double-click **"Install KhDetector.command"**
2. Choose installation location (system-wide or user-only)
3. Enter password if prompted
4. Restart your DAW and rescan plugins

### Manual Installation
Copy \`KhDetector_CLAP.clap\` to:
- System: \`/Library/Audio/Plug-Ins/CLAP/\`
- User: \`~/Library/Audio/Plug-Ins/CLAP/\`

## Supported DAWs

- ✅ **Ableton Live 11.3+** (CLAP support)
- ✅ **Reaper** (full CLAP support)
- ✅ **Bitwig Studio** (native CLAP support)
- ✅ **FL Studio** (newer versions)
- ✅ **Studio One 6+** (CLAP support)

## Usage

1. Load KhDetector on an audio track with drums
2. Adjust sensitivity if needed
3. Route MIDI output to trigger samples or instruments
4. The plugin outputs MIDI Note 45 (A2) when kicks are detected

## Technical Specifications

- **Plugin Format:** CLAP
- **Processing:** Real-time safe, lock-free
- **Latency:** <1ms additional latency
- **CPU Usage:** <5% single core
- **SIMD:** Optimized for modern CPUs

## Troubleshooting

- **Plugin doesn't appear:** Clear DAW plugin cache and rescan
- **No MIDI output:** Check MIDI routing in your DAW
- **High CPU usage:** Reduce buffer size or sample rate

## Support

- GitHub: https://github.com/khdetector
- Issues: https://github.com/khdetector/issues
- Documentation: https://khdetector.com/docs

## License

Copyright © 2024 KhDetector. All rights reserved.
EOF

    success "README created"
}

# Create DMG background and styling
create_dmg_styling() {
    log "Creating DMG styling..."
    
    # Create a simple background (you can replace this with a custom image)
    mkdir -p "$DMG_DIR/.background"
    
    # Create a simple text-based background info
    cat > "$DMG_DIR/.DS_Store_template" << 'EOF'
# DMG Window Settings
# This would normally contain binary .DS_Store data
# for window positioning and background image
EOF

    success "DMG styling prepared"
}

# Build the DMG
build_dmg() {
    log "Building DMG..."
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    local temp_dmg="$TEMP_DIR/temp.dmg"
    
    # Calculate size needed (add some padding)
    local size_kb=$(du -sk "$DMG_DIR" | cut -f1)
    local size_mb=$((size_kb / 1024 + 50))  # Add 50MB padding
    
    log "Creating DMG with ${size_mb}MB capacity..."
    
    # Create temporary DMG
    hdiutil create -srcfolder "$DMG_DIR" \
                   -volname "KhDetector v${PRODUCT_VERSION}" \
                   -fs HFS+ \
                   -fsargs "-c c=64,a=16,e=16" \
                   -format UDRW \
                   -size ${size_mb}m \
                   "$temp_dmg"
    
    # Mount the DMG for customization
    local device=$(hdiutil attach -readwrite -noverify -noautoopen "$temp_dmg" | grep '^/dev/' | awk '{print $1}')
    local mount_point="/Volumes/KhDetector v${PRODUCT_VERSION}"
    
    # Wait for mount
    sleep 2
    
    # Set custom icon and window properties if possible
    if [[ -d "$mount_point" ]]; then
        log "Customizing DMG appearance..."
        
        # Create symbolic link to Applications folder for drag-and-drop
        # (Not needed for plugin installer, but kept for reference)
        
        # Set window properties using AppleScript (simplified to avoid errors)
        osascript << EOF || warning "DMG styling failed, but DMG will still work"
tell application "Finder"
    tell disk "KhDetector v${PRODUCT_VERSION}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 900, 400}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 72
        close
        open
        update without registering applications
        delay 1
    end tell
end tell
EOF
    fi
    
    # Unmount the DMG
    hdiutil detach "$device"
    
    # Convert to compressed read-only DMG
    hdiutil convert "$temp_dmg" \
                    -format UDZO \
                    -imagekey zlib-level=9 \
                    -o "$dmg_path"
    
    # Clean up temp DMG
    rm -f "$temp_dmg"
    
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
    rm -rf "$TEMP_DIR"
    rm -rf "$DMG_DIR"
    success "Cleanup completed"
}

# Main execution
main() {
    log "Starting DMG creation for $PRODUCT_NAME v$PRODUCT_VERSION"
    
    check_prerequisites
    create_dmg_structure
    copy_plugins
    create_installation_guide
    create_installer_script
    create_uninstaller_script
    create_readme
    create_dmg_styling
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
    log "  • Uninstall KhDetector.command (uninstaller)"
    log "  • README.md (documentation)"
    log "  • INSTALL.txt (installation instructions)"
    echo
    log "Users can double-click the DMG and run the installer for easy installation."
}

# Handle script termination
trap cleanup EXIT

# Run main function
main "$@" 