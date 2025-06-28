# Husher - Hebrew ח Detection VST Plugin

A real-time VST3/AU plugin that automatically detects Hebrew consonant ח in audio recordings, marking detections on the DAW timeline.

## Quick Start

### 1. Clone with Submodules
```bash
git clone --recursive https://github.com/yossiferoz/Husher.git
cd Husher
```

### 2. Build
```bash
mkdir build && cd build
cmake ..
make -j4
```

### 3. Install
The build process automatically installs the plugins to:
- **VST3**: `~/Library/Audio/Plug-Ins/VST3/Husher.vst3`
- **AU**: `~/Library/Audio/Plug-Ins/Components/Husher.component`

## Features

- Real-time Hebrew ח consonant detection
- Confidence meter with adjustable sensitivity
- MIDI marker output for DAW timeline
- Cross-platform VST3/AU support
- JUCE-based professional audio plugin framework

## Development

- Built with JUCE framework
- Future AI detection via ONNX Runtime
- Cross-platform CMake build system

## Troubleshooting

**Plugin not found in DAW:**
1. Ensure you ran `git clone --recursive` to download JUCE submodule
2. Verify plugins installed in correct directories (see above)
3. Restart your DAW after building

**Build issues:**
```bash
# If JUCE submodule missing:
git submodule update --init --recursive
```