#!/bin/bash

# Quick Ableton Live Plugin Installer
# Installs KhDetector in multiple formats for maximum compatibility

set -euo pipefail

echo "🎵 KhDetector Plugin Installer for Ableton Live"
echo "==============================================="
echo

# Check if plugin exists
if [[ ! -d "build_clap/KhDetector_CLAP.clap" ]]; then
    echo "❌ Error: Plugin not found at build_clap/KhDetector_CLAP.clap"
    echo "Please run the build process first."
    exit 1
fi

echo "📦 Installing KhDetector plugin..."

# 1. Install CLAP plugin
echo "1. Installing CLAP plugin..."
mkdir -p ~/Library/Audio/Plug-Ins/CLAP/
cp -R build_clap/KhDetector_CLAP.clap ~/Library/Audio/Plug-Ins/CLAP/
chmod -R 755 ~/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap
echo "   ✅ CLAP plugin installed to ~/Library/Audio/Plug-Ins/CLAP/"

# 2. Create Audio Unit wrapper
echo "2. Creating Audio Unit wrapper..."
mkdir -p ~/Library/Audio/Plug-Ins/Components/
cp -R build_clap/KhDetector_CLAP.clap ~/Library/Audio/Plug-Ins/Components/KhDetector.component
echo "   ✅ AU wrapper created at ~/Library/Audio/Plug-Ins/Components/"

# 3. Create VST3 wrapper
echo "3. Creating VST3 wrapper..."
mkdir -p ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/

# Copy the binary
cp build_clap/KhDetector_CLAP.clap/Contents/MacOS/KhDetector_CLAP \
   ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/KhDetector

# Create Info.plist
cat > ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/Info.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>KhDetector</string>
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
EOF

chmod +x ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/KhDetector
echo "   ✅ VST3 wrapper created at ~/Library/Audio/Plug-Ins/VST3/"

# 4. Clear caches
echo "4. Clearing plugin caches..."
rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null || true
rm -rf ~/Library/Caches/VST* 2>/dev/null || true
rm -rf ~/Library/Caches/Ableton* 2>/dev/null || true
echo "   ✅ Plugin caches cleared"

echo
echo "🎉 Installation Complete!"
echo
echo "📋 Next Steps:"
echo "1. 🔄 Restart Ableton Live completely"
echo "2. ⚙️  Go to Live > Preferences > Plug-Ins"
echo "3. 🔍 Click 'Rescan' to refresh plugin database"
echo "4. 🎛️  Look for 'KhDetector' in:"
echo "   - Browser > Plug-Ins > Audio Effects"
echo "   - Browser > Plug-Ins > CLAP (if supported)"
echo "   - Browser > Plug-Ins > VST3"
echo
echo "🎯 Plugin Features:"
echo "   - Real-time kick drum detection"
echo "   - MIDI output: Note 45 (A2)"
echo "   - Parameters: Sensitivity, Bypass, Hit Detected"
echo
echo "❓ If plugin doesn't appear:"
echo "   - Check Live > Preferences > Plug-Ins folder settings"
echo "   - Try the plugin in Reaper or Bitwig Studio first"
echo "   - Check Console.app for plugin loading errors"
echo

# Ask to kill Ableton Live if running
if pgrep -f "Ableton Live" > /dev/null; then
    echo "🔄 Ableton Live is currently running."
    read -p "Kill Ableton Live to apply changes? (y/N): " kill_ableton
    
    if [[ "$kill_ableton" =~ ^[Yy]$ ]]; then
        pkill -f "Ableton Live" || true
        echo "   ✅ Ableton Live terminated. You can now restart it."
    else
        echo "   ⚠️  Please restart Ableton Live manually to see the plugin."
    fi
fi

echo
echo "✨ Installation completed successfully!" 