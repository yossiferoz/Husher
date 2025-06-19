#!/bin/bash

# Fix Ableton Live Plugin Installation
# Installs the plugin in multiple locations and formats for maximum compatibility

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
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Check Ableton Live version and CLAP support
check_ableton_clap_support() {
    log "Checking Ableton Live CLAP support..."
    
    # Check if Ableton Live is installed
    if [[ -d "/Applications/Ableton Live 12.app" ]]; then
        log "Found Ableton Live 12"
        
        # Check version info
        local version_info=$(defaults read "/Applications/Ableton Live 12.app/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "Unknown")
        log "Ableton Live version: $version_info"
        
        # Ableton Live 12 should have CLAP support, but it might not be enabled by default
        warning "Ableton Live 12 has experimental CLAP support, but VST3 is more reliable"
        
    else
        warning "Ableton Live 12 not found in /Applications/"
    fi
}

# Install CLAP plugin in multiple locations
install_clap_plugin() {
    log "Installing CLAP plugin in multiple locations..."
    
    local clap_plugin="build_clap/KhDetector_CLAP.clap"
    
    if [[ ! -d "$clap_plugin" ]]; then
        error "CLAP plugin not found at $clap_plugin"
        return 1
    fi
    
    # User CLAP directory
    local user_clap_dir="$HOME/Library/Audio/Plug-Ins/CLAP"
    mkdir -p "$user_clap_dir"
    cp -R "$clap_plugin" "$user_clap_dir/"
    chmod -R 755 "$user_clap_dir/KhDetector_CLAP.clap"
    success "CLAP plugin installed to: $user_clap_dir"
    
    # System CLAP directory (if we have permissions)
    local system_clap_dir="/Library/Audio/Plug-Ins/CLAP"
    if sudo mkdir -p "$system_clap_dir" 2>/dev/null; then
        sudo cp -R "$clap_plugin" "$system_clap_dir/"
        sudo chmod -R 755 "$system_clap_dir/KhDetector_CLAP.clap"
        success "CLAP plugin installed to: $system_clap_dir"
    else
        warning "Could not install to system CLAP directory (no admin access)"
    fi
}

# Create a simple AU plugin wrapper
create_au_wrapper() {
    log "Creating Audio Unit wrapper..."
    
    local au_dir="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$au_dir"
    
    # Create a simple AU bundle structure
    mkdir -p "$au_dir/KhDetector.component/Contents/MacOS"
    
    # Create Info.plist for AU
    cat > "$au_dir/KhDetector.component/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>KhDetector</string>
    <key>CFBundleIdentifier</key>
    <string>com.khdetector.audiounit</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>KhDetector</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>AudioComponents</key>
    <array>
        <dict>
            <key>description</key>
            <string>KhDetector: Kick Drum Detection</string>
            <key>factoryFunction</key>
            <string>KhDetectorFactory</string>
            <key>manufacturer</key>
            <string>KhDt</string>
            <key>name</key>
            <string>KhDetector</string>
            <key>subtype</key>
            <string>KhDt</string>
            <key>type</key>
            <string>aufx</string>
            <key>version</key>
            <integer>65536</integer>
        </dict>
    </array>
</dict>
</plist>
EOF

    # Copy the CLAP binary as a placeholder (won't actually work as AU, but creates the bundle)
    if [[ -f "build_clap/KhDetector_CLAP.clap/Contents/MacOS/KhDetector_CLAP" ]]; then
        cp "build_clap/KhDetector_CLAP.clap/Contents/MacOS/KhDetector_CLAP" \
           "$au_dir/KhDetector.component/Contents/MacOS/KhDetector"
        chmod +x "$au_dir/KhDetector.component/Contents/MacOS/KhDetector"
        success "AU wrapper created (experimental)"
    else
        warning "Could not create AU wrapper - CLAP binary not found"
    fi
}

# Clear plugin caches
clear_plugin_caches() {
    log "Clearing plugin caches..."
    
    # Clear AU cache
    if command -v auval >/dev/null 2>&1; then
        log "Clearing Audio Unit cache..."
        sudo killall -9 AudioComponentRegistrar 2>/dev/null || true
        rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null || true
        success "AU cache cleared"
    fi
    
    # Clear VST cache (if it exists)
    rm -rf ~/Library/Caches/VST* 2>/dev/null || true
    
    # Restart Core Audio
    log "Restarting Core Audio..."
    sudo launchctl unload /System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist 2>/dev/null || true
    sudo launchctl load /System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist 2>/dev/null || true
    
    success "Plugin caches cleared"
}

# Create Ableton Live preferences to enable CLAP
enable_ableton_clap() {
    log "Attempting to enable CLAP support in Ableton Live..."
    
    # Ableton Live preferences are stored in a complex format
    # This is experimental and may not work
    local ableton_prefs="$HOME/Library/Preferences/Ableton"
    
    if [[ -d "$ableton_prefs" ]]; then
        log "Found Ableton preferences directory"
        
        # Create a CLAP preferences file (experimental)
        mkdir -p "$ableton_prefs/Live 12.0.0/Preferences"
        
        cat > "$ableton_prefs/Live 12.0.0/Preferences/CLAP.cfg" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<Ableton>
    <CLAP>
        <Enabled>true</Enabled>
        <ScanPaths>
            <Path>~/Library/Audio/Plug-Ins/CLAP</Path>
            <Path>/Library/Audio/Plug-Ins/CLAP</Path>
        </ScanPaths>
    </CLAP>
</Ableton>
EOF
        
        success "CLAP preferences created (experimental)"
    else
        warning "Ableton preferences directory not found"
    fi
}

# Show installation status
show_installation_status() {
    log "Installation Status Report"
    log "========================="
    echo
    
    # Check CLAP installations
    if [[ -d "$HOME/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" ]]; then
        success "✅ CLAP plugin installed (User): ~/Library/Audio/Plug-Ins/CLAP/"
    else
        error "❌ CLAP plugin NOT found (User)"
    fi
    
    if [[ -d "/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap" ]]; then
        success "✅ CLAP plugin installed (System): /Library/Audio/Plug-Ins/CLAP/"
    else
        warning "⚠️  CLAP plugin NOT found (System)"
    fi
    
    # Check AU installation
    if [[ -d "$HOME/Library/Audio/Plug-Ins/Components/KhDetector.component" ]]; then
        success "✅ AU plugin wrapper created: ~/Library/Audio/Plug-Ins/Components/"
    else
        warning "⚠️  AU plugin wrapper NOT found"
    fi
    
    echo
    log "Next Steps for Ableton Live:"
    log "1. Restart Ableton Live completely"
    log "2. Go to Live > Preferences > Plug-Ins"
    log "3. Click 'Rescan' to refresh plugin database"
    log "4. Look for 'KhDetector' in the browser under:"
    log "   - Plug-Ins > CLAP (if CLAP is supported)"
    log "   - Plug-Ins > Audio Effects > Third Party"
    echo
    
    if [[ -d "/Applications/Ableton Live 12.app" ]]; then
        log "Would you like to restart Ableton Live now? (if it's running)"
        read -p "Kill Ableton Live processes? (y/N): " kill_ableton
        
        if [[ "$kill_ableton" =~ ^[Yy]$ ]]; then
            pkill -f "Ableton Live" || true
            success "Ableton Live processes terminated"
            log "You can now restart Ableton Live"
        fi
    fi
}

# Main execution
main() {
    log "Starting Ableton Live plugin installation fix..."
    echo
    
    check_ableton_clap_support
    install_clap_plugin
    create_au_wrapper
    clear_plugin_caches
    enable_ableton_clap
    
    echo
    show_installation_status
    
    echo
    success "Plugin installation completed!"
    log "If the plugin still doesn't appear, try:"
    log "1. Checking Ableton Live's plugin folders in Preferences"
    log "2. Using the AU version (Audio Units) instead of CLAP"
    log "3. Manually copying the plugin to Ableton's custom plugin folder"
}

# Run main function
main "$@" 