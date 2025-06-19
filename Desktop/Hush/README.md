# KhDetector VST3 Plugin

A minimal C++17 cross-platform VST3/AU/AAX/CLAP plugin project using CMake and the Steinberg VST3 SDK.

## Features

- **Cross-platform**: macOS (x86_64 + arm64 Universal), Windows (x64), Linux (x64)
- **Plugin Formats**: VST3 (using Steinberg VST3 SDK)
- **Build System**: CMake 3.20+
- **C++ Standard**: C++17
- **CI/CD**: GitHub Actions for automated builds

## Project Structure

```
KhDetector/
├── CMakeLists.txt              # Main CMake configuration
├── .gitmodules                 # Git submodules (VST3 SDK)
├── .github/workflows/          # GitHub Actions CI/CD
├── cmake/                      # CMake templates and modules
│   └── Info.plist.in          # macOS bundle info template
├── external/                   # External dependencies
│   └── vst3sdk/               # Steinberg VST3 SDK (submodule)
├── src/                       # Plugin source code
│   ├── KhDetectorProcessor.h   # Audio processor header
│   ├── KhDetectorProcessor.cpp # Audio processor implementation
│   ├── KhDetectorController.h  # Parameter controller header
│   ├── KhDetectorController.cpp# Parameter controller implementation
│   ├── KhDetectorFactory.cpp   # Plugin factory registration
│   ├── KhDetectorVersion.h     # Version and GUID definitions
│   ├── RingBuffer.h           # Lock-free ring buffer template
│   └── PolyphaseDecimator.h   # SIMD-optimized decimator
└── tests/                     # Unit tests
    ├── test_ringbuffer.cpp    # RingBuffer unit tests
    └── test_decimator.cpp     # PolyphaseDecimator unit tests
```

## Prerequisites

### macOS
- Xcode 12.0 or later
- CMake 3.20 or later

### Windows
- Visual Studio 2019 or later (with C++ support)
- CMake 3.20 or later

### Linux
- GCC 9.0+ or Clang 10.0+
- CMake 3.20 or later
- Build essentials

## Building

### Quick Start (Recommended)

The easiest way to build is using the provided build scripts:

**macOS/Linux:**
```bash
./build.sh --debug --install --test
```

**Windows:**
```cmd
build.bat --debug --install --test
```

### Build Script Options

- `-r, --release`: Build in Release mode
- `-d, --debug`: Build in Debug mode (default)
- `-c, --clean`: Clean build directory before building
- `-i, --install`: Install plugin to system directory after building
- `-t, --test`: Run unit tests after building
- `-h, --help`: Show help message

### Manual Building

#### Clone with Submodules

```bash
git clone --recursive https://github.com/yourusername/KhDetector.git
cd KhDetector

# If you forgot --recursive:
git submodule update --init --recursive
```

#### macOS

```bash
# Configure (Universal Binary)
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

# Build
cmake --build build --config Debug --parallel

# Install to system plugin directory (optional)
cmake --build build --target install
```

#### Windows

```bash
# Configure
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Debug --parallel
```

#### Linux

```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug --parallel
```

## Plugin Architecture

The plugin follows the VST3 architecture with separate Processor and Controller classes:

### KhDetectorProcessor
- Inherits from `Steinberg::Vst::AudioEffect`
- Handles audio processing with decimation pipeline
- Downsamples stereo 48kHz/44.1kHz input to mono 16kHz
- Uses polyphase FIR decimator with SIMD optimization
- Enqueues 20ms frames (320 samples) in lock-free ring buffer
- Configurable decimation factor via `DECIM_FACTOR` compile option

### KhDetectorController
- Inherits from `Steinberg::Vst::EditControllerEx1`
- Manages plugin parameters and GUI
- Handles preset save/load functionality
- Currently includes a bypass parameter

### RingBuffer Template
- Lock-free single-producer/single-consumer ring buffer
- Template class supporting any trivially copyable type
- Optimized for real-time audio applications
- Thread-safe without blocking operations
- Supports bulk operations for efficiency
- Power-of-2 sizes for optimal performance

### PolyphaseDecimator
- SIMD-optimized polyphase FIR decimator for efficient downsampling
- Supports AVX, SSE2/SSE4.1, and ARM NEON instruction sets
- Windowed sinc lowpass filter design with anti-aliasing
- Configurable decimation factor (compile-time option)
- Stereo-to-mono conversion with simultaneous decimation
- Real-time safe with predictable performance

### RealtimeThreadPool
- Fixed-size thread pool based on CPU core count (cores - 1)
- Configurable thread priority below audio thread
- Processes 20ms audio frames from ring buffer
- Integrates with AI inference engine for ML processing
- Real-time safe task scheduling without blocking audio
- Cross-platform thread priority management
- Comprehensive performance monitoring and statistics
- Graceful handling of queue overflow and thread lifecycle

### AiInference (Stub Implementation)
- Placeholder AI inference engine for audio processing
- Realistic dummy processing for testing and development
- Configurable model parameters and normalization
- Performance monitoring with inference statistics  
- Thread-safe processing with callback support
- Easy integration point for actual ML frameworks
- Hardware capability detection (GPU/CPU)
- Warmup functionality for consistent performance
- Integrated post-processing pipeline for hit detection

### PostProcessor
- Median filter smoothing for confidence value noise reduction
- Configurable threshold detection with hysteresis support
- Boolean hit state generation with std::atomic<bool> thread safety
- Minimum hit duration filtering to prevent false positives
- Debouncing to prevent rapid successive hit triggering
- Maximum hit duration with automatic reset functionality
- Comprehensive statistics tracking and event callbacks
- Multiple configuration presets (sensitive, robust, fast)
- Real-time safe processing with predictable performance
- Thread-safe atomic operations for GUI/controller integration

## Testing

The project includes comprehensive unit tests using GoogleTest:

### Running Tests

**With build scripts:**
```bash
# macOS/Linux
./build.sh --test

# Windows
build.bat --test
```

**Manual testing:**
```bash
# Build tests
cmake --build build --target KhDetectorTests

# Run tests with CTest
cd build && ctest --verbose

# Or run directly
./build/KhDetectorTests  # macOS/Linux
build\Debug\KhDetectorTests.exe  # Windows

# Build with custom decimation factor
cmake -DDECIM_FACTOR=4 -B build -S .  # For 4:1 decimation
```

### Test Coverage

**RingBuffer Tests:**
- **Basic functionality**: Push, pop, peek operations
- **Boundary conditions**: Empty, full buffer states
- **Data integrity**: FIFO ordering and data preservation
- **Thread safety**: Single-producer/single-consumer scenarios
- **Performance**: Bulk operations and stress testing
- **Type safety**: Different data types and move semantics

**PolyphaseDecimator Tests:**
- **Signal processing**: Mono and stereo-to-mono decimation
- **Filter response**: Anti-aliasing and DC response verification
- **SIMD optimization**: Cross-platform instruction set usage
- **Streaming consistency**: Chunk-based vs. continuous processing
- **Noise handling**: Stability with random input signals
- **Multiple factors**: Different decimation ratios (2x, 3x, 4x)

**RealtimeThreadPool Tests:**
- **Thread management**: Creation, startup, shutdown lifecycle
- **Task scheduling**: General purpose task submission and execution
- **AI processing**: Integration with inference engine and ring buffer
- **Performance monitoring**: Statistics collection and CPU usage tracking
- **Thread safety**: Concurrent access and queue overflow handling
- **Priority management**: Different priority levels across platforms
- **Real-time constraints**: Processing interval timing and frame-based operation
- **Stress testing**: Long-running operations and rapid start/stop cycles
- **Error handling**: Invalid parameters and graceful degradation

**PostProcessor Tests:**
- **Median filtering**: Noise reduction and outlier suppression
- **Hit detection**: Threshold-based boolean state generation
- **Configuration validation**: Automatic parameter correction and validation
- **Hysteresis thresholding**: Stable switching with dual thresholds
- **Duration filtering**: Minimum/maximum hit duration enforcement
- **Debouncing**: Prevention of rapid successive hit triggering
- **Statistics accuracy**: Performance monitoring and event tracking
- **Callback functionality**: Event notifications and state changes
- **Thread safety**: Atomic operations and concurrent access
- **Performance benchmarking**: Processing speed with different filter sizes
- **Synthetic data testing**: Various confidence patterns and edge cases

**MidiEventHandler Tests:**
- **MIDI note generation**: Note on/off events for hit detection
- **State tracking**: Prevents duplicate events for same state
- **Configuration validation**: Automatic correction of invalid MIDI values
- **Timing accuracy**: Host timestamp handling and sample-accurate events
- **Delayed note off**: Configurable delays and scheduled event processing
- **Statistics monitoring**: Event counting and performance tracking
- **Cross-platform compatibility**: VST3/AAX/AU API preparation
- **Performance testing**: High-frequency state change handling
- **Edge case handling**: Boundary conditions and error scenarios

## Customization

### Plugin Information
Edit `src/KhDetectorVersion.h` to customize:
- Plugin name and vendor information
- Version numbers
- Unique plugin GUIDs (generate new ones!)

### Adding Parameters
1. Add parameter IDs to the `Parameters` enum in both header files
2. Register parameters in `KhDetectorController::initialize()`
3. Handle parameter changes in `KhDetectorProcessor::process()`

### Audio Processing
Implement your audio processing logic in:
- `KhDetectorProcessor::process()` - Main processing function
- `KhDetectorProcessor::processAudio()` - Template for different sample types

### Using the RingBuffer
```cpp
#include "RingBuffer.h"

// Create a ring buffer for audio samples
KhDetector::RingBuffer<float, 1024> audioBuffer;

// Producer thread (e.g., audio callback)
float sample = 0.5f;
if (audioBuffer.push(sample)) {
    // Successfully pushed sample
}

// Consumer thread (e.g., GUI update)
float receivedSample;
if (audioBuffer.pop(receivedSample)) {
    // Successfully received sample
}

// Bulk operations for efficiency
std::vector<float> samples(256);
size_t pushed = audioBuffer.push_bulk(samples.data(), samples.size());
size_t popped = audioBuffer.pop_bulk(samples.data(), samples.size());
```

### Using the PolyphaseDecimator
```cpp
#include "PolyphaseDecimator.h"

// Create decimator for 48kHz -> 16kHz (factor of 3)
KhDetector::PolyphaseDecimator<3, 48> decimator;

// Process stereo input to mono decimated output
std::vector<float> leftChannel(480);   // 10ms at 48kHz
std::vector<float> rightChannel(480);
std::vector<float> decimatedOutput(160); // 10ms at 16kHz

int outputSamples = decimator.processStereoToMono(
    leftChannel.data(), rightChannel.data(),
    decimatedOutput.data(), leftChannel.size()
);

// Configure decimation factor at build time
// cmake -DDECIM_FACTOR=3 ...
```

### Using the MIDI Event Handler
```cpp
#include "MidiEventHandler.h"

// Create MIDI handler with custom configuration
KhDetector::MidiEventHandler::Config config;
config.hitNote = 45;        // A2
config.hitVelocity = 127;   // Maximum velocity
config.channel = 0;         // MIDI channel 1 (0-based)
config.sendNoteOff = true;  // Send note off when hit ends
config.noteOffDelay = 0;    // Immediate note off

auto midiHandler = std::make_unique<KhDetector::MidiEventHandler>(config);

// In audio processing loop
bool currentHit = detectHit();  // Your hit detection logic
uint64_t hostTimeStamp = getHostTimeStamp();
int32_t sampleOffset = getCurrentSampleOffset();

// Process hit state changes
auto* midiEvent = midiHandler->processHitState(currentHit, sampleOffset, hostTimeStamp);
if (midiEvent) {
#ifdef VST3_SUPPORT
    // Send VST3 MIDI event
    KhDetector::MidiEventHandler::sendVST3Event(outputEvents, *midiEvent);
#endif
    
    std::cout << "MIDI Event: " << (midiEvent->type == KhDetector::MidiEventHandler::EventType::NoteOn ? "Note ON" : "Note OFF") 
              << " - Note " << static_cast<int>(midiEvent->note) 
              << " (" << KhDetector::midiNoteToString(midiEvent->note) << ")" << std::endl;
}

// Check for delayed note off events
auto* pendingEvent = midiHandler->processPendingEvents(currentSamplePosition);
if (pendingEvent) {
#ifdef VST3_SUPPORT
    KhDetector::MidiEventHandler::sendVST3Event(outputEvents, *pendingEvent);
#endif
}

// Monitor statistics
auto stats = midiHandler->getStatistics();
std::cout << "MIDI Events: " << stats.totalEvents 
          << " (On: " << stats.noteOnEvents << ", Off: " << stats.noteOffEvents << ")" << std::endl;
```

### Using the RealtimeThreadPool with AI Inference
```cpp
#include "RealtimeThreadPool.h"
#include "AiInference.h"
#include "RingBuffer.h"

// Create components
KhDetector::RingBuffer<float, 2048> audioBuffer;
auto aiConfig = KhDetector::createDefaultModelConfig();
aiConfig.inputSize = 320;  // 20ms frames at 16kHz
auto aiInference = KhDetector::createAiInference(aiConfig);

// Create thread pool with low priority (below audio thread)
auto threadPool = std::make_unique<KhDetector::RealtimeThreadPool>(
    KhDetector::RealtimeThreadPool::Priority::Low, 
    320  // Frame size
);

// Start processing (typically in plugin setActive)
threadPool->start(&audioBuffer, aiInference.get(), 20); // 20ms intervals

// Audio thread fills the ring buffer (non-blocking)
float audioSample = getNextAudioSample();
audioBuffer.push(audioSample);

// AI inference runs automatically in background threads
// Monitor performance
auto stats = threadPool->getStatistics();
std::cout << "Frames processed: " << stats.framesProcessed.load() << std::endl;
std::cout << "CPU usage: " << stats.cpuUsagePercent.load() << "%" << std::endl;

// Stop processing (typically in plugin setActive(false))
threadPool->stop();
```

### Running Examples
The project includes demonstration programs:

```bash
# Build and run examples
cmake --build build --target examples

# Ring buffer demonstration
./build/examples/ring_buffer_demo

# Decimation pipeline demonstration  
./build/examples/decimation_demo

# Thread pool and AI inference demonstration
./build/examples/threadpool_demo [duration_seconds]

# MIDI event handling demonstration
./build/examples/midi_demo

# Post-processing with median filtering demonstration
./build/examples/postprocessor_demo

# OpenGL GUI demonstration (without actual GUI context)
./build/examples/opengl_gui_demo
```

### OpenGL GUI Components

The plugin features an advanced OpenGL-based GUI with real-time visualization:

#### KhDetectorOpenGLView
- **Real-time FPS counter**: Displays current and average frame rates
- **Hit detection flash**: Visual feedback with smooth animations
- **OpenGL rendering**: Hardware-accelerated graphics with custom shaders
- **Performance monitoring**: Frame count, hit statistics, and timing data
- **Configurable appearance**: Colors, flash duration, refresh rates
- **Cross-platform OpenGL**: Works on macOS, Windows, and Linux

#### KhDetectorGUIView
- **Composite view**: Combines OpenGL rendering with text overlays
- **Text displays**: FPS counter, hit counter, and statistics
- **Real-time updates**: Separate update rates for graphics and text
- **VSTGUI integration**: Native text rendering and font management
- **Thread-safe**: Atomic access to processor hit state
- **Configurable layout**: Customizable fonts, colors, and positioning

#### KhDetectorEditor
- **VST3 editor integration**: Proper plugin GUI lifecycle management
- **Hit state monitoring**: Real-time connection to processor atomic state
- **Platform support**: Cross-platform OpenGL context management
- **Resource management**: Automatic cleanup and memory management

#### Using the OpenGL GUI
```cpp
#include "KhDetectorGUIView.h"

// Create GUI view with hit state reference
std::atomic<bool> hitState{false};
VSTGUI::CRect viewSize(0, 0, 600, 400);
auto guiView = createGUIView(viewSize, hitState);

// Configure GUI appearance
KhDetectorGUIView::Config config;
config.showFPS = true;
config.showHitCounter = true;
config.textUpdateRate = 10.0f;      // 10 Hz for text
config.openglUpdateRate = 60.0f;    // 60 Hz for OpenGL
config.fontSize = 14.0f;
guiView->setConfig(config);

// Start real-time updates
guiView->startUpdates();

// Trigger hit detection (from audio thread)
hitState.store(true);  // Flash will appear in GUI
```

#### OpenGL GUI Features
- **Hardware acceleration**: Utilizes GPU for smooth animations
- **Real-time performance**: 60+ FPS rendering with minimal CPU usage
- **Hit visualization**: Immediate visual feedback for detected hits
- **Statistics display**: Live monitoring of processing performance
- **Cross-platform**: Consistent appearance across operating systems
- **Thread-safe**: Safe atomic communication with audio processing thread

**OpenGL GUI Tests:**
- **View creation**: OpenGL and composite GUI view instantiation
- **Configuration management**: Appearance settings and update rates
- **Hit detection integration**: Atomic state monitoring and visualization
- **Animation control**: Start/stop functionality and resource management
- **Performance monitoring**: FPS calculation and statistics tracking
- **Thread safety**: Concurrent access to shared atomic state
- **Factory functions**: Creation utilities and proper initialization
- **Configuration persistence**: Settings validation and application

## License

This project is provided as-is for educational and development purposes. The Steinberg VST3 SDK has its own license terms which must be followed.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on all target platforms
5. Submit a pull request

## Support

For issues and questions:
- Check the [VST3 SDK documentation](https://steinbergmedia.github.io/vst3_doc/)
- Review the [CMake documentation](https://cmake.org/documentation/)
- Open an issue in this repository 