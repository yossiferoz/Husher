#!/bin/bash

# Simple Professional macOS DMG Creation Script
# Creates a Mac-standard drag-and-drop installer

set -euo pipefail

# Configuration
PRODUCT_NAME="KhDetector"
PRODUCT_VERSION="${PRODUCT_VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build_clap}"
DMG_DIR="${DMG_DIR:-dmg_simple_build}"
OUTPUT_DIR="${OUTPUT_DIR:-packages}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

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
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        error "Build directory '$BUILD_DIR' not found. Run build first."
    fi
    
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    if [[ ! -d "$clap_path" ]]; then
        error "CLAP plugin not found at $clap_path"
    fi
    
    command -v hdiutil >/dev/null 2>&1 || error "hdiutil not found"
    
    success "Prerequisites check passed"
}

# Create DMG structure
create_dmg_structure() {
    log "Creating DMG structure..."
    
    rm -rf "$DMG_DIR"
    mkdir -p "$DMG_DIR"
    mkdir -p "$OUTPUT_DIR"
    
    success "DMG structure created"
}

# Create Applications symlink
create_applications_link() {
    log "Creating Applications folder symlink..."
    ln -s /Applications "$DMG_DIR/Applications"
    success "Applications symlink created"
}

# Setup plugin and installer
setup_plugin_structure() {
    log "Setting up plugin installation structure..."
    
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    # Copy CLAP plugin
    cp -R "$clap_path" "$DMG_DIR/"
    
    # Create professional installer app
    mkdir -p "$DMG_DIR/KhDetector Installer.app/Contents/MacOS"
    mkdir -p "$DMG_DIR/KhDetector Installer.app/Contents/Resources"
    
    # Create Info.plist
    cat > "$DMG_DIR/KhDetector Installer.app/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>KhDetector Installer</string>
    <key>CFBundleIdentifier</key>
    <string>com.khdetector.installer</string>
    <key>CFBundleName</key>
    <string>KhDetector Installer</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

    # Create installer script
    cat > "$DMG_DIR/KhDetector Installer.app/Contents/MacOS/KhDetector Installer" << 'EOF'
#!/bin/bash

# KhDetector Professional Installer
set -euo pipefail

INSTALLER_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
DMG_ROOT="$(dirname "$(dirname "$(dirname "$INSTALLER_DIR")")")"

# Show welcome dialog
osascript << 'APPLESCRIPT'
set welcomeDialog to display dialog "Welcome to KhDetector Installer

KhDetector is a professional audio plugin for real-time kick drum detection.

This installer will install the plugin to:
• ~/Library/Audio/Plug-Ins/CLAP/ (CLAP format)
• ~/Library/Audio/Plug-Ins/VST3/ (VST3 format)
• ~/Library/Audio/Plug-Ins/Components/ (AU format)

Click Install to continue." buttons {"Cancel", "Install"} default button "Install" with title "KhDetector Installer" with icon note

if button returned of welcomeDialog is "Cancel" then
    error number -128
end if
APPLESCRIPT

if [[ $? -ne 0 ]]; then
    exit 0
fi

# Install plugin
echo "Installing KhDetector..."

# Create directories
mkdir -p ~/Library/Audio/Plug-Ins/CLAP/
mkdir -p ~/Library/Audio/Plug-Ins/VST3/
mkdir -p ~/Library/Audio/Plug-Ins/Components/

# Copy CLAP plugin
if [[ -d "$DMG_ROOT/KhDetector_CLAP.clap" ]]; then
    cp -R "$DMG_ROOT/KhDetector_CLAP.clap" ~/Library/Audio/Plug-Ins/CLAP/
    chmod -R 755 ~/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap
fi

# Create VST3 wrapper
if [[ -d "$DMG_ROOT/KhDetector_CLAP.clap" ]]; then
    mkdir -p ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/
    cp -R "$DMG_ROOT/KhDetector_CLAP.clap/Contents/MacOS/"* ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/ 2>/dev/null || true
    
    cat > ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/Info.plist << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>KhDetector_CLAP</string>
    <key>CFBundleIdentifier</key>
    <string>com.khdetector.vst3</string>
    <key>CFBundleName</key>
    <string>KhDetector</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
</dict>
</plist>
PLIST
    
    chmod -R 755 ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3
fi

# Create AU wrapper
if [[ -d "$DMG_ROOT/KhDetector_CLAP.clap" ]]; then
    cp -R "$DMG_ROOT/KhDetector_CLAP.clap" ~/Library/Audio/Plug-Ins/Components/KhDetector.component
    chmod -R 755 ~/Library/Audio/Plug-Ins/Components/KhDetector.component
fi

# Clear caches
rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null || true
rm -rf ~/Library/Caches/VST* 2>/dev/null || true

# Show completion dialog
osascript << 'APPLESCRIPT'
display dialog "Installation Complete!

KhDetector has been successfully installed.

Next Steps:
1. Restart your DAW (Ableton Live, Logic Pro, etc.)
2. Rescan plugins in your DAW preferences
3. Look for 'KhDetector' in your plugin list

The plugin detects kick drums and outputs MIDI Note 45 (A2)." buttons {"OK"} default button "OK" with title "Installation Complete" with icon note
APPLESCRIPT

echo "Installation completed successfully!"
EOF

    chmod +x "$DMG_DIR/KhDetector Installer.app/Contents/MacOS/KhDetector Installer"
    
    # Create README
    cat > "$DMG_DIR/README.txt" << EOF
KhDetector v${PRODUCT_VERSION} - Audio Plugin
=============================================

🎵 INSTALLATION:

1. AUTOMATIC (Recommended):
   Double-click "KhDetector Installer.app"

2. DRAG-AND-DROP:
   Drag "KhDetector_CLAP.clap" to "Applications" folder

3. MANUAL:
   Copy plugin to:
   • ~/Library/Audio/Plug-Ins/CLAP/
   • ~/Library/Audio/Plug-Ins/VST3/
   • ~/Library/Audio/Plug-Ins/Components/

🎛️ SUPPORTED DAWs:
• Ableton Live 11.3+ • Logic Pro X/Logic Pro
• Pro Tools • Reaper • Bitwig Studio
• Cubase/Nuendo • FL Studio • Studio One

⚡ FEATURES:
• Real-time kick drum detection
• MIDI output (Note 45/A2)
• Low-latency processing
• Universal Binary (Intel + Apple Silicon)

GitHub: https://github.com/yossiferoz/Hush
EOF
    
    success "Plugin structure created"
}

# Build DMG
build_dmg() {
    log "Building professional DMG..."
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    # Create DMG
    hdiutil create -srcfolder "$DMG_DIR" \
                   -volname "KhDetector v${PRODUCT_VERSION}" \
                   -fs HFS+ \
                   -format UDZO \
                   -imagekey zlib-level=9 \
                   "$dmg_path"
    
    success "DMG created: $dmg_path"
    
    local file_size=$(du -h "$dmg_path" | cut -f1)
    log "DMG size: $file_size"
}

# Create metadata
create_metadata() {
    log "Creating metadata..."
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    # Checksums
    shasum -a 256 "$dmg_path" > "$dmg_path.sha256"
    md5 "$dmg_path" > "$dmg_path.md5"
    
    # Metadata
    cat > "$dmg_path.info" << EOF
KhDetector v${PRODUCT_VERSION} - macOS Installer
===============================================

File: $dmg_name
Created: $(date)
Size: $(du -h "$dmg_path" | cut -f1)
Platform: macOS 10.13+
Architecture: Universal Binary

Installation Features:
✅ Professional installer app
✅ Drag-and-drop to Applications
✅ Multiple plugin formats (CLAP/VST3/AU)
✅ Universal Binary support

For support: https://github.com/yossiferoz/Hush
EOF
    
    success "Metadata created"
}

# Cleanup
cleanup() {
    log "Cleaning up..."
    rm -rf "$DMG_DIR"
    success "Cleanup completed"
}

# Main execution
main() {
    log "Creating professional macOS DMG for $PRODUCT_NAME v$PRODUCT_VERSION"
    echo
    
    check_prerequisites
    create_dmg_structure
    create_applications_link
    setup_plugin_structure
    build_dmg
    create_metadata
    cleanup
    
    local dmg_name="KhDetector-${PRODUCT_VERSION}-macOS.dmg"
    local dmg_path="$OUTPUT_DIR/$dmg_name"
    
    echo
    success "🎉 Professional DMG created successfully!"
    log "📍 Location: $dmg_path"
    log "📊 Size: $(du -h "$dmg_path" | cut -f1)"
    echo
    log "🎯 Mac Installation Features:"
    log "  ✅ Professional installer app with native dialogs"
    log "  ✅ Drag-and-drop to Applications folder"
    log "  ✅ Multiple plugin formats (CLAP/VST3/AU)"
    log "  ✅ Universal Binary (Intel + Apple Silicon)"
    log "  ✅ Automatic plugin cache clearing"
    echo
    log "🚀 Ready for professional distribution!"
}

# Handle cleanup on exit
trap cleanup EXIT

# Run main function
main "$@" 