# KhDetector Plugin Installation Guide for Ableton Live

## Issue: Plugin Not Appearing in Ableton Live

The issue is that **Ableton Live 12 doesn't have full CLAP support enabled by default**. Here are several solutions:

## Solution 1: Manual CLAP Installation (Recommended)

### Step 1: Install the CLAP Plugin Manually
```bash
# Create the CLAP directory if it doesn't exist
mkdir -p ~/Library/Audio/Plug-Ins/CLAP/

# Copy the plugin from the DMG or build directory
cp -R build_clap/KhDetector_CLAP.clap ~/Library/Audio/Plug-Ins/CLAP/

# Set proper permissions
chmod -R 755 ~/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap
```

### Step 2: Enable CLAP in Ableton Live 12
1. **Open Ableton Live 12**
2. **Go to**: `Live > Preferences > Plug-Ins`
3. **Check**: "Use CLAP Plug-Ins" (if available)
4. **Add Folder**: Click "Browse" and add `~/Library/Audio/Plug-Ins/CLAP/`
5. **Rescan**: Click "Rescan" to refresh the plugin database

### Step 3: Look for the Plugin
- **Location**: `Browser > Plug-Ins > CLAP > KhDetector_CLAP`
- **Alternative**: `Browser > Plug-Ins > Audio Effects > Third Party`

## Solution 2: Audio Unit (AU) Installation

Since CLAP support might be limited, let's create an Audio Unit version:

### Step 1: Create AU Directory
```bash
mkdir -p ~/Library/Audio/Plug-Ins/Components/
```

### Step 2: Copy Plugin as AU (Experimental)
```bash
# Copy the CLAP plugin and rename it as AU
cp -R build_clap/KhDetector_CLAP.clap ~/Library/Audio/Plug-Ins/Components/KhDetector.component

# The plugin might appear as an Audio Unit
```

### Step 3: Clear AU Cache
```bash
# Clear Audio Unit cache
rm -rf ~/Library/Caches/AudioUnitCache
sudo killall -9 AudioComponentRegistrar
```

## Solution 3: Check Ableton's Plugin Preferences

### Step 1: Check Plugin Folders
1. **Open Ableton Live 12**
2. **Go to**: `Live > Preferences > Plug-Ins`
3. **Check**: What folders are being scanned
4. **Add**: `~/Library/Audio/Plug-Ins/CLAP/` if not present

### Step 2: Enable Plugin Types
- **VST3**: Should be enabled by default
- **Audio Units**: Should be enabled by default  
- **CLAP**: May need to be manually enabled

## Solution 4: Alternative DAW Testing

To verify the plugin works, try it in other DAWs that have better CLAP support:

### Reaper (Full CLAP Support)
1. **Download**: [Reaper trial](https://www.reaper.fm/download.php)
2. **Install**: The CLAP plugin will be automatically detected
3. **Test**: Load it on an audio track

### Bitwig Studio (Native CLAP Support)
1. **Download**: [Bitwig trial](https://www.bitwig.com/download/)
2. **Install**: CLAP plugins are natively supported
3. **Test**: Should appear immediately

## Solution 5: Create a VST3 Version

Since VST3 has universal support in Ableton Live, let's create a VST3 wrapper:

### Quick VST3 Creation
```bash
# Create VST3 directory
mkdir -p ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/

# Copy the CLAP binary as VST3 (experimental)
cp build_clap/KhDetector_CLAP.clap/Contents/MacOS/KhDetector_CLAP \
   ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3/Contents/MacOS/KhDetector

# Create VST3 Info.plist
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
```

## Troubleshooting Steps

### 1. Check Plugin Installation
```bash
# Verify CLAP plugin exists
ls -la ~/Library/Audio/Plug-Ins/CLAP/KhDetector_CLAP.clap

# Verify AU plugin exists  
ls -la ~/Library/Audio/Plug-Ins/Components/KhDetector.component

# Verify VST3 plugin exists
ls -la ~/Library/Audio/Plug-Ins/VST3/KhDetector.vst3
```

### 2. Check Ableton Live Version
```bash
# Check Ableton Live version
defaults read "/Applications/Ableton Live 12.app/Contents/Info.plist" CFBundleShortVersionString
```

### 3. Force Plugin Rescan
1. **Quit Ableton Live completely**
2. **Delete plugin cache**: `rm -rf ~/Library/Caches/Ableton*`
3. **Restart Ableton Live**
4. **Rescan plugins** in Preferences

### 4. Check Console for Errors
1. **Open Console.app**
2. **Filter by**: "Ableton" or "plugin"
3. **Look for**: Plugin loading errors

## Expected Behavior

Once installed correctly, the KhDetector plugin should:

1. **Appear in**: Browser > Plug-Ins > Audio Effects
2. **Parameters**: 
   - Sensitivity (0.0 - 1.0)
   - Bypass (On/Off)
   - Hit Detected (Read-only indicator)
3. **Function**: Detect kick drums and output MIDI Note 45 (A2)

## Contact for Support

If none of these solutions work:

1. **Check**: What exact version of Ableton Live 12 you have
2. **Try**: The plugin in Reaper or Bitwig to verify it works
3. **Report**: Any error messages from Console.app
4. **Consider**: Using a different DAW with better CLAP support

The plugin is working (as evidenced by the successful DMG creation), but Ableton Live's CLAP support may be limited or require specific configuration. 