#!/bin/bash

# macOS Packaging Script for KhDetector
# Creates a notarized .pkg installer for distribution

set -euo pipefail

# Configuration
PRODUCT_NAME="KhDetector"
PRODUCT_VERSION="${PRODUCT_VERSION:-1.0.0}"
BUNDLE_ID="com.khdetector.audio.plugin"
DEVELOPER_ID="${DEVELOPER_ID:-Developer ID Installer: KhDetector Audio (DUMMY123)}"
NOTARIZATION_PROFILE="${NOTARIZATION_PROFILE:-khdetector-notarization}"
BUILD_DIR="${BUILD_DIR:-build}"
PACKAGE_DIR="${PACKAGE_DIR:-packages}"
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
    command -v pkgbuild >/dev/null 2>&1 || error "pkgbuild not found"
    command -v productbuild >/dev/null 2>&1 || error "productbuild not found"
    command -v codesign >/dev/null 2>&1 || error "codesign not found"
    command -v xcrun >/dev/null 2>&1 || error "xcrun not found"
    
    # Check for built plugins
    local vst3_path="$BUILD_DIR/VST3/KhDetector.vst3"
    local au_path="$BUILD_DIR/AU/KhDetector.component"
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    if [[ ! -d "$vst3_path" ]] && [[ ! -d "$au_path" ]] && [[ ! -d "$clap_path" ]]; then
        error "No plugin formats found in build directory"
    fi
    
    success "Prerequisites check passed"
}

# Create package structure
create_package_structure() {
    log "Creating package structure..."
    
    mkdir -p "$PACKAGE_DIR"
    mkdir -p "$TEMP_DIR/payload"
    mkdir -p "$TEMP_DIR/scripts"
    mkdir -p "$TEMP_DIR/resources"
    
    # Create standard macOS plugin directories
    mkdir -p "$TEMP_DIR/payload/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$TEMP_DIR/payload/Library/Audio/Plug-Ins/Components"
    mkdir -p "$TEMP_DIR/payload/Library/Audio/Plug-Ins/CLAP"
    mkdir -p "$TEMP_DIR/payload/Applications/KhDetector"
    
    success "Package structure created"
}

# Sign plugins
sign_plugins() {
    log "Signing plugins..."
    
    local vst3_path="$BUILD_DIR/VST3/KhDetector.vst3"
    local au_path="$BUILD_DIR/AU/KhDetector.component"
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    # Sign VST3
    if [[ -d "$vst3_path" ]]; then
        log "Signing VST3 plugin..."
        codesign --force --sign "$DEVELOPER_ID" \
                 --options runtime \
                 --timestamp \
                 --identifier "$BUNDLE_ID.vst3" \
                 "$vst3_path"
        
        # Verify signature
        codesign --verify --verbose "$vst3_path"
        success "VST3 plugin signed"
    fi
    
    # Sign AU
    if [[ -d "$au_path" ]]; then
        log "Signing AU plugin..."
        codesign --force --sign "$DEVELOPER_ID" \
                 --options runtime \
                 --timestamp \
                 --identifier "$BUNDLE_ID.component" \
                 "$au_path"
        
        codesign --verify --verbose "$au_path"
        success "AU plugin signed"
    fi
    
    # Sign CLAP
    if [[ -d "$clap_path" ]]; then
        log "Signing CLAP plugin..."
        codesign --force --sign "$DEVELOPER_ID" \
                 --options runtime \
                 --timestamp \
                 --identifier "$BUNDLE_ID.clap" \
                 "$clap_path"
        
        codesign --verify --verbose "$clap_path"
        success "CLAP plugin signed"
    fi
}

# Copy plugins to package
copy_plugins() {
    log "Copying plugins to package..."
    
    local vst3_path="$BUILD_DIR/VST3/KhDetector.vst3"
    local au_path="$BUILD_DIR/AU/KhDetector.component"
    local clap_path="$BUILD_DIR/KhDetector_CLAP.clap"
    
    # Copy VST3
    if [[ -d "$vst3_path" ]]; then
        cp -R "$vst3_path" "$TEMP_DIR/payload/Library/Audio/Plug-Ins/VST3/"
        log "VST3 plugin copied"
    fi
    
    # Copy AU
    if [[ -d "$au_path" ]]; then
        cp -R "$au_path" "$TEMP_DIR/payload/Library/Audio/Plug-Ins/Components/"
        log "AU plugin copied"
    fi
    
    # Copy CLAP
    if [[ -d "$clap_path" ]]; then
        cp -R "$clap_path" "$TEMP_DIR/payload/Library/Audio/Plug-Ins/CLAP/"
        log "CLAP plugin copied"
    fi
    
    # Copy demo applications if they exist
    if [[ -f "$BUILD_DIR/opengl_gui_demo" ]]; then
        cp "$BUILD_DIR/opengl_gui_demo" "$TEMP_DIR/payload/Applications/KhDetector/"
        log "Demo applications copied"
    fi
    
    success "Plugins copied to package"
}

# Create installer scripts
create_installer_scripts() {
    log "Creating installer scripts..."
    
    # Pre-install script
    cat > "$TEMP_DIR/scripts/preinstall" << 'EOF'
#!/bin/bash

# Pre-install script for KhDetector
echo "Preparing to install KhDetector..."

# Kill any running DAWs that might have the plugin loaded
pkill -f "Logic Pro" || true
pkill -f "Pro Tools" || true
pkill -f "Cubase" || true
pkill -f "Nuendo" || true
pkill -f "Studio One" || true
pkill -f "Reaper" || true
pkill -f "Ableton Live" || true

# Remove old versions
rm -rf "/Library/Audio/Plug-Ins/VST3/KhDetector.vst3" || true
rm -rf "/Library/Audio/Plug-Ins/Components/KhDetector.component" || true
rm -rf "/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" || true

exit 0
EOF

    # Post-install script
    cat > "$TEMP_DIR/scripts/postinstall" << 'EOF'
#!/bin/bash

# Post-install script for KhDetector
echo "Finalizing KhDetector installation..."

# Set proper permissions
chmod -R 755 "/Library/Audio/Plug-Ins/VST3/KhDetector.vst3" || true
chmod -R 755 "/Library/Audio/Plug-Ins/Components/KhDetector.component" || true
chmod -R 755 "/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" || true

# Clear plugin caches
rm -rf ~/Library/Caches/AudioUnitCache || true
rm -rf /Library/Caches/AudioUnitCache || true

# Restart CoreAudio to recognize new plugins
sudo launchctl kickstart -k system/com.apple.audio.coreaudiod || true

echo "KhDetector installation completed successfully!"
echo "Please restart your DAW to use the new plugins."

exit 0
EOF

    chmod +x "$TEMP_DIR/scripts/preinstall"
    chmod +x "$TEMP_DIR/scripts/postinstall"
    
    success "Installer scripts created"
}

# Create package resources
create_package_resources() {
    log "Creating package resources..."
    
    # Welcome text
    cat > "$TEMP_DIR/resources/Welcome.txt" << EOF
Welcome to KhDetector ${PRODUCT_VERSION}

KhDetector is a professional audio plugin for kick drum detection and analysis.

This installer will install the following plugin formats:
• VST3 - Compatible with most modern DAWs
• Audio Unit (AU) - Compatible with Logic Pro, GarageBand, and other AU hosts
• CLAP - Next-generation plugin format

System Requirements:
• macOS 10.13 or later
• 64-bit Intel or Apple Silicon processor
• Compatible DAW software

Please close all audio applications before proceeding with the installation.
EOF

    # License text
    cat > "$TEMP_DIR/resources/License.txt" << EOF
KhDetector Software License Agreement

Copyright (c) 2024 KhDetector Audio. All rights reserved.

This software is provided "as is" without warranty of any kind, express or implied.

By installing this software, you agree to the terms and conditions of this license.

For full license terms, please visit: https://khdetector.com/license
EOF

    # ReadMe text
    cat > "$TEMP_DIR/resources/ReadMe.txt" << EOF
KhDetector ${PRODUCT_VERSION} Installation

Installation Locations:
• VST3: /Library/Audio/Plug-Ins/VST3/KhDetector.vst3
• Audio Unit: /Library/Audio/Plug-Ins/Components/KhDetector.component
• CLAP: /Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap

After installation:
1. Restart your DAW
2. Rescan plugins if necessary
3. Look for "KhDetector" in your plugin list

For support and documentation:
https://khdetector.com/support

Troubleshooting:
If plugins don't appear, try clearing your DAW's plugin cache and rescanning.
EOF

    success "Package resources created"
}

# Build component package
build_component_package() {
    log "Building component package..."
    
    local component_pkg="$TEMP_DIR/KhDetector-component.pkg"
    
    pkgbuild --root "$TEMP_DIR/payload" \
             --scripts "$TEMP_DIR/scripts" \
             --identifier "$BUNDLE_ID" \
             --version "$PRODUCT_VERSION" \
             --install-location "/" \
             "$component_pkg"
    
    success "Component package built"
}

# Build distribution package
build_distribution_package() {
    log "Building distribution package..."
    
    local distribution_xml="$TEMP_DIR/distribution.xml"
    local final_pkg="$PACKAGE_DIR/KhDetector-${PRODUCT_VERSION}-macOS.pkg"
    
    # Create distribution XML
    cat > "$distribution_xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>KhDetector ${PRODUCT_VERSION}</title>
    <organization>com.khdetector.audio</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="true" rootVolumeOnly="true"/>
    
    <!-- Welcome -->
    <welcome file="Welcome.txt" mime-type="text/plain"/>
    
    <!-- License -->
    <license file="License.txt" mime-type="text/plain"/>
    
    <!-- ReadMe -->
    <readme file="ReadMe.txt" mime-type="text/plain"/>
    
    <!-- Background -->
    <background file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    
    <!-- Choices -->
    <choices-outline>
        <line choice="default">
            <line choice="com.khdetector.audio.plugin"/>
        </line>
    </choices-outline>
    
    <choice id="default"/>
    <choice id="com.khdetector.audio.plugin" visible="false">
        <pkg-ref id="com.khdetector.audio.plugin"/>
    </choice>
    
    <pkg-ref id="com.khdetector.audio.plugin" version="$PRODUCT_VERSION" onConclusion="none">KhDetector-component.pkg</pkg-ref>
    
</installer-gui-script>
EOF

    # Build the final package
    productbuild --distribution "$distribution_xml" \
                 --resources "$TEMP_DIR/resources" \
                 --package-path "$TEMP_DIR" \
                 "$final_pkg"
    
    success "Distribution package built: $final_pkg"
}

# Sign the final package
sign_package() {
    log "Signing final package..."
    
    local final_pkg="$PACKAGE_DIR/KhDetector-${PRODUCT_VERSION}-macOS.pkg"
    
    # Sign the package
    productsign --sign "$DEVELOPER_ID" \
                --timestamp \
                "$final_pkg" \
                "$final_pkg.signed"
    
    # Replace unsigned with signed
    mv "$final_pkg.signed" "$final_pkg"
    
    # Verify signature
    pkgutil --check-signature "$final_pkg"
    
    success "Package signed successfully"
}

# Notarize package
notarize_package() {
    log "Submitting package for notarization..."
    
    local final_pkg="$PACKAGE_DIR/KhDetector-${PRODUCT_VERSION}-macOS.pkg"
    
    # Submit for notarization
    xcrun notarytool submit "$final_pkg" \
                           --keychain-profile "$NOTARIZATION_PROFILE" \
                           --wait
    
    if [[ $? -eq 0 ]]; then
        success "Package notarized successfully"
        
        # Staple the notarization
        xcrun stapler staple "$final_pkg"
        success "Notarization stapled to package"
    else
        warning "Notarization failed or skipped (CI environment)"
    fi
}

# Create checksums
create_checksums() {
    log "Creating checksums..."
    
    local final_pkg="$PACKAGE_DIR/KhDetector-${PRODUCT_VERSION}-macOS.pkg"
    
    # SHA256
    shasum -a 256 "$final_pkg" > "$final_pkg.sha256"
    
    # MD5
    md5 "$final_pkg" > "$final_pkg.md5"
    
    success "Checksums created"
}

# Cleanup
cleanup() {
    log "Cleaning up temporary files..."
    rm -rf "$TEMP_DIR"
    success "Cleanup completed"
}

# Main execution
main() {
    log "Starting macOS packaging for $PRODUCT_NAME v$PRODUCT_VERSION"
    
    check_prerequisites
    create_package_structure
    sign_plugins
    copy_plugins
    create_installer_scripts
    create_package_resources
    build_component_package
    build_distribution_package
    sign_package
    
    # Only notarize if not in CI or if specifically requested
    if [[ "${CI:-false}" != "true" ]] || [[ "${FORCE_NOTARIZATION:-false}" == "true" ]]; then
        notarize_package
    else
        warning "Skipping notarization in CI environment"
    fi
    
    create_checksums
    cleanup
    
    local final_pkg="$PACKAGE_DIR/KhDetector-${PRODUCT_VERSION}-macOS.pkg"
    success "macOS package created successfully!"
    log "Package location: $final_pkg"
    log "Package size: $(du -h "$final_pkg" | cut -f1)"
}

# Handle script termination
trap cleanup EXIT

# Run main function
main "$@" 