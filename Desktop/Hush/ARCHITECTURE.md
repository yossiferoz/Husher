# KhDetector Architecture Documentation

This document provides a comprehensive overview of the KhDetector audio plugin architecture, covering the audio processing pipeline, AI inference system, threading model, and UI rendering pipeline.

## Table of Contents
1. [System Overview](#system-overview)
2. [Audio Processing Pipeline](#audio-processing-pipeline)
3. [AI Inference Path](#ai-inference-path)
4. [Threading Model](#threading-model)
5. [UI Render Pipeline](#ui-render-pipeline)
6. [Component Diagrams](#component-diagrams)
7. [Data Flow](#data-flow)
8. [Performance Characteristics](#performance-characteristics)

## System Overview

KhDetector is a cross-platform audio plugin for real-time kick drum detection using AI inference. The system is designed around a lock-free, real-time safe architecture that separates high-priority audio processing from lower-priority AI computation.

### Key Design Principles
- **Real-time Safety**: Audio thread never blocks or allocates memory
- **Lock-free Communication**: Ring buffers for inter-thread data exchange
- **SIMD Optimization**: Vectorized signal processing for performance
- **Modular Architecture**: Loosely coupled components with clear interfaces
- **Cross-platform**: VST3, AU, CLAP support on macOS, Windows, Linux

## Audio Processing Pipeline

The audio processing pipeline transforms stereo input audio into 20ms mono frames suitable for AI analysis.

### Pipeline Stages

1. **Stereo Input** (48kHz, variable buffer size)
   - Host provides stereo audio buffers
   - Typical buffer sizes: 64-2048 samples

2. **Stereo-to-Mono Conversion**
   - Simple averaging: `mono = (left + right) * 0.5`
   - Performed in real-time audio thread

3. **Polyphase Decimation** (48kHz → 16kHz)
   - SIMD-optimized FIR filtering with 3:1 decimation
   - Anti-aliasing filter prevents frequency folding
   - Group delay: 8 samples at 16kHz (~0.5ms)

4. **Ring Buffer Queuing**
   - Lock-free circular buffer (2048 samples capacity)
   - Real-time thread writes, AI thread reads
   - Overflow protection with sample dropping

5. **Frame Assembly** (320 samples = 20ms at 16kHz)
   - AI thread collects samples into analysis frames
   - 50% overlap for temporal continuity
   - Frame-based processing for ML model compatibility

### Signal Flow Diagram

```
Stereo Input (48kHz) → Mono Conversion → Polyphase Decimator → Ring Buffer → Frame Assembly → AI Inference
    ↓                      ↓                    ↓                  ↓             ↓
Pass-through          Real-time safe       SIMD optimized    Lock-free      20ms frames
to output             averaging            anti-aliasing     queuing        (320 samples)
```

## AI Inference Path

The AI inference system processes audio frames to detect kick drum events using a dedicated thread pool.

### Inference Pipeline

1. **Frame Preprocessing**
   - Normalization and windowing
   - Feature extraction (if required)
   - Input validation and bounds checking

2. **Model Inference**
   - Placeholder for ML framework integration
   - Designed for TensorFlow Lite, PyTorch Mobile, or ONNX Runtime
   - Configurable input/output dimensions

3. **Post-processing**
   - Confidence thresholding
   - Temporal smoothing with median filter
   - Hit detection logic

4. **Event Generation**
   - MIDI note output (Note 45, Velocity 127)
   - Sample-accurate timing
   - GUI state updates

### Inference Configuration

```cpp
struct ModelConfig {
    std::string modelPath;           // Path to model file
    int inputSize = 320;             // 20ms frames at 16kHz
    int outputSize = 1;              // Binary classification
    float sampleRate = 16000.0f;     // Expected sample rate
    float confidenceThreshold = 0.5f; // Detection threshold
    bool useGpu = false;             // GPU acceleration
    int numThreads = 1;              // Inference threads
};
```

## Threading Model

The system uses a carefully designed threading architecture to maintain real-time performance while enabling complex AI processing.

### Thread Hierarchy

1. **Audio Thread** (Highest Priority)
   - Real-time constraints: ~1-10ms deadlines
   - No memory allocation or blocking operations
   - Minimal processing: decimation + ring buffer writes
   - Platform-specific real-time scheduling

2. **AI Processing Thread** (Medium Priority)
   - Consumes frames from ring buffer
   - Runs AI inference asynchronously
   - 20ms processing intervals
   - CPU cores-1 thread pool size

3. **GUI Thread** (Normal Priority)
   - OpenGL rendering at 60-120 FPS
   - Waveform visualization updates
   - User interaction handling
   - Timer-based refresh cycles

4. **Background Threads** (Low Priority)
   - File I/O operations
   - Model loading/initialization
   - Statistics collection

### Thread Communication

```
Audio Thread ──[Ring Buffer]──→ AI Thread ──[Atomic Flags]──→ GUI Thread
     ↓                              ↓                           ↑
Pass-through                   MIDI Events                 Visualization
Audio Output                   to Host                     Updates
```

### Synchronization Mechanisms

- **Lock-free Ring Buffers**: Audio → AI data transfer
- **Atomic Variables**: State flags (hit detection, bypass)
- **Memory Barriers**: Ensure ordering without locks
- **Thread-local Storage**: Per-thread processing buffers

## UI Render Pipeline

The OpenGL-based GUI provides real-time waveform visualization with high-performance rendering.

### Rendering Architecture

1. **Waveform Data Collection**
   - Circular buffer (4K samples)
   - Thread-safe sample collection
   - Time-windowed data extraction

2. **Vertex Generation**
   - Convert audio samples to OpenGL vertices
   - Apply time-based scrolling transforms
   - Generate spectral overlay data

3. **VBO Double-buffering**
   - Two vertex buffer objects for smooth updates
   - Write to back buffer while rendering front buffer
   - 50ms line recycling for SMAA optimization

4. **Shader Pipeline**
   - Vertex shader: position transforms, scrolling
   - Fragment shader: color blending, anti-aliasing
   - Uniform variables: time offset, amplitude scaling

5. **Post-processing**
   - SMAA (Subpixel Morphological Anti-Aliasing)
   - Hit flash overlays
   - FPS counter rendering

### Rendering Performance

- **Target Frame Rate**: 120 FPS
- **VSync**: Disabled for low latency
- **SMAA**: Enabled for smooth line rendering
- **Double Buffering**: Prevents visual tearing
- **Viewport Scaling**: Automatic DPI adaptation

## Component Diagrams

### High-Level System Architecture

```plantuml
@startuml KhDetector_System_Architecture
!theme plain
skinparam componentStyle rectangle
skinparam backgroundColor #FAFAFA
skinparam component {
    BackgroundColor #E1F5FE
    BorderColor #0277BD
    FontSize 10
}
skinparam interface {
    BackgroundColor #FFF3E0
    BorderColor #FF8F00
}
skinparam note {
    BackgroundColor #F1F8E9
    BorderColor #689F38
}

package "Host Application (DAW)" {
    [Audio Engine] as Host
    [MIDI Sequencer] as MIDI
    [Plugin Scanner] as Scanner
}

package "KhDetector Plugin" {
    package "Plugin Formats" {
        [VST3 Wrapper] as VST3
        [Audio Unit] as AU
        [CLAP Wrapper] as CLAP
    }
    
    package "Core Processing" {
        [KhDetectorProcessor] as Processor
        [PolyphaseDecimator] as Decimator
        [RingBuffer] as RingBuf
        [RealtimeThreadPool] as ThreadPool
        [AiInference] as AI
    }
    
    package "Audio Analysis" {
        [PostProcessor] as PostProc
        [MidiEventHandler] as MidiHandler
        [SpectralAnalyzer] as Spectral
    }
    
    package "User Interface" {
        [KhDetectorController] as Controller
        [OpenGLView] as GUI
        [WaveformRenderer] as Renderer
        [WaveformBuffer] as WaveBuffer
    }
}

package "External Dependencies" {
    [VST3 SDK] as VST3SDK
    [CLAP SDK] as CLAPSDK
    [OpenGL] as GL
    [VSTGUI] as VSTGUI
}

' Host connections
Host --> VST3 : Audio I/O
Host --> AU : Audio I/O  
Host --> CLAP : Audio I/O
MIDI <-- VST3 : MIDI Events
MIDI <-- AU : MIDI Events
MIDI <-- CLAP : MIDI Events

' Plugin format connections
VST3 --> Processor : process()
AU --> Processor : process()
CLAP --> Processor : process()

' Core processing flow
Processor --> Decimator : Stereo → Mono
Decimator --> RingBuf : Decimated Samples
RingBuf --> ThreadPool : Frame Data
ThreadPool --> AI : 20ms Frames
AI --> PostProc : Predictions
PostProc --> MidiHandler : Hit Events
MidiHandler --> Processor : MIDI Output

' GUI connections
Controller --> GUI : Parameter Updates
Processor --> WaveBuffer : Audio Samples
WaveBuffer --> Renderer : Waveform Data
Renderer --> GUI : OpenGL Rendering
Spectral --> Renderer : Spectral Overlay

' External dependencies
VST3 ..> VST3SDK : uses
CLAP ..> CLAPSDK : uses
GUI ..> GL : uses
GUI ..> VSTGUI : uses

note right of ThreadPool
  CPU cores-1 threads
  Real-time priority
  20ms processing intervals
end note

note right of RingBuf
  Lock-free circular buffer
  2048 sample capacity
  Real-time safe
end note

note bottom of AI
  Placeholder for ML frameworks:
  • TensorFlow Lite
  • PyTorch Mobile  
  • ONNX Runtime
end note

@enduml
```

### Audio Processing Pipeline

```plantuml
@startuml Audio_Processing_Pipeline
!theme plain
skinparam backgroundColor #FAFAFA
skinparam activity {
    BackgroundColor #E8F5E8
    BorderColor #4CAF50
    FontSize 10
}
skinparam note {
    BackgroundColor #FFF3E0
    BorderColor #FF8F00
}

|Audio Thread|
start
:Receive Stereo Input\n(48kHz, variable buffer);
note right: Host provides audio buffers\n64-2048 samples typical

:Convert Stereo to Mono\nmono = (left + right) * 0.5;
note right: Real-time safe operation\nNo memory allocation

:Polyphase Decimation\n48kHz → 16kHz (3:1);
note right: SIMD-optimized FIR filter\nAnti-aliasing protection\nGroup delay: 8 samples

:Write to Ring Buffer\n(Lock-free);
note right: 2048 sample circular buffer\nOverflow protection\nAtomic operations

:Pass-through to Output\n(Unchanged audio);
note right: Plugin is transparent\nto audio signal

|AI Thread|
:Read from Ring Buffer\n(Non-blocking);
note left: Consume decimated samples\nNo blocking operations

if (Enough samples for frame?) then (yes)
    :Assemble 20ms Frame\n(320 samples at 16kHz);
    note left: Frame-based processing\n50% overlap for continuity
    
    :AI Inference\n(Model prediction);
    note left: Placeholder for ML model\nConfigurable architecture
    
    :Post-processing\n(Median filter, thresholding);
    note left: Temporal smoothing\nConfidence validation
    
    if (Hit detected?) then (yes)
        :Generate MIDI Event\n(Note 45, Velocity 127);
        note left: Sample-accurate timing\nQueued for host
        
        :Update GUI State\n(Atomic flag);
        note left: Lock-free state update\nVisual feedback
    endif
else (no)
    :Wait for more samples;
endif

|GUI Thread|
:Update Waveform Display\n(60-120 FPS);
note right: OpenGL rendering\nVBO double-buffering

:Render Hit Indicators\n(Flash effects);
note right: Visual feedback\nTimed animations

stop

@enduml
```

### Threading Model Detail

```plantuml
@startuml Threading_Model
!theme plain
skinparam backgroundColor #FAFAFA
skinparam participant {
    BackgroundColor #E3F2FD
    BorderColor #1976D2
}
skinparam note {
    BackgroundColor #F1F8E9
    BorderColor #689F38
}

participant "Host\nAudio Thread" as Host
participant "KhDetector\nAudio Thread" as Audio
participant "Ring Buffer\n(Lock-free)" as Ring
participant "AI Thread Pool\n(CPU-1 threads)" as AI
participant "GUI Thread\n(OpenGL)" as GUI
participant "Background\nThreads" as BG

note over Host: Real-time Priority\n~1-10ms deadlines
note over Audio: Real-time Priority\nNo allocations/locks
note over AI: Medium Priority\n20ms intervals
note over GUI: Normal Priority\n60-120 FPS
note over BG: Low Priority\nFile I/O, stats

Host -> Audio: process(stereo_input, buffer_size)
activate Audio

Audio -> Audio: stereo_to_mono()
Audio -> Audio: polyphase_decimation()
Audio -> Ring: push(decimated_samples)
note right: Atomic operations\nOverflow protection

Audio -> Host: return(processed_audio)
deactivate Audio

loop AI Processing Loop
    AI -> Ring: pop_bulk(frame_data, 320)
    note right: Non-blocking read\nFrame assembly
    
    alt Enough samples available
        AI -> AI: ai_inference(frame)
        AI -> AI: post_processing()
        
        alt Hit detected
            AI -> Host: queue_midi_event()
            AI -> GUI: set_hit_flag(true)
            note right: Atomic state update
        end
    else Not enough samples
        AI -> AI: sleep(1ms)
    end
end

loop GUI Rendering Loop
    GUI -> GUI: collect_waveform_data()
    GUI -> GUI: generate_vertices()
    GUI -> GUI: render_opengl()
    GUI -> GUI: swap_buffers()
    note right: VBO double-buffering\nSMAA anti-aliasing
end

loop Background Tasks
    BG -> BG: update_statistics()
    BG -> BG: file_operations()
    BG -> BG: cleanup_tasks()
end

@enduml
```

### Data Flow Architecture

```plantuml
@startuml Data_Flow_Architecture
!theme plain
skinparam backgroundColor #FAFAFA
skinparam class {
    BackgroundColor #E8F5E8
    BorderColor #4CAF50
    FontSize 10
}
skinparam interface {
    BackgroundColor #FFF3E0
    BorderColor #FF8F00
}
skinparam note {
    BackgroundColor #E1F5FE
    BorderColor #0277BD
}

class "Stereo Audio Input" as Input {
    +float* left_channel
    +float* right_channel
    +int buffer_size
    +double sample_rate
}

class "PolyphaseDecimator" as Decimator {
    +processStereoToMono()
    +SIMD optimization
    +Anti-aliasing filter
    -circular_buffer[48]
    -filter_coefficients[48]
}

class "RingBuffer<float, 2048>" as RingBuffer {
    +push(sample) : bool
    +pop_bulk(buffer, count) : size_t
    +size() : size_t
    -atomic operations
    -lock-free design
}

class "RealtimeThreadPool" as ThreadPool {
    +start(ring_buffer, ai_inference)
    +stop()
    +getStatistics()
    -worker_threads[CPU-1]
    -processing_interval: 20ms
}

class "AiInference" as AI {
    +run(frame_data, size) : Result
    +warmup(iterations)
    +isReady() : bool
    -model_config
    -inference_engine
}

class "PostProcessor" as PostProc {
    +process(predictions) : bool
    +setThreshold(value)
    -median_filter
    -confidence_threshold
}

class "MidiEventHandler" as MIDI {
    +generateHitEvent() : MidiEvent
    +reset()
    -note: 45
    -velocity: 127
    -channel: 0
}

class "WaveformBuffer4K" as WaveBuffer {
    +addSample(amplitude, timestamp)
    +getSamplesInTimeWindow() : vector
    +clear()
    -circular_buffer[4096]
    -thread_safe
}

class "WaveformRenderer" as Renderer {
    +render(samples, spectral_frames)
    +setViewport(width, height)
    -vbo_manager
    -shader_program
    -smaa_helper
}

interface "Plugin Host Interface" as HostInterface {
    +process(ProcessData&)
    +setState(IBStream*)
    +getState(IBStream*)
}

' Data flow connections
Input --> Decimator : "Stereo samples\n48kHz"
Decimator --> RingBuffer : "Mono samples\n16kHz"
RingBuffer --> ThreadPool : "Frame data\n320 samples"
ThreadPool --> AI : "20ms frames"
AI --> PostProc : "Predictions\nConfidence scores"
PostProc --> MIDI : "Hit events\nBinary decisions"
MIDI --> HostInterface : "MIDI Note 45\nVelocity 127"

' Parallel data flows
Input --> WaveBuffer : "Raw audio\nVisualization"
WaveBuffer --> Renderer : "Waveform data\nTime windows"
Renderer --> HostInterface : "OpenGL rendering\nGUI updates"

' Configuration flows
HostInterface --> Decimator : "Sample rate\nBuffer size"
HostInterface --> AI : "Model config\nParameters"
HostInterface --> PostProc : "Threshold\nSensitivity"

note top of RingBuffer
Lock-free circular buffer
- Real-time safe operations
- Atomic read/write pointers
- Overflow protection
- Memory barriers for ordering
end note

note right of ThreadPool
Thread Pool Management
- CPU cores-1 worker threads
- Real-time priority scheduling
- 20ms processing intervals
- Load balancing
end note

note bottom of AI
AI Inference Engine
- Placeholder for ML frameworks
- Configurable model architecture
- GPU acceleration support
- Batch processing capability
end note

@enduml
```

## Data Flow

### Real-time Audio Path
1. **Host → Plugin**: Stereo audio buffers (48kHz)
2. **Decimation**: Polyphase FIR filter (48kHz → 16kHz)
3. **Ring Buffer**: Lock-free sample queuing
4. **Frame Assembly**: 320-sample frames (20ms)
5. **AI Inference**: Model prediction
6. **Post-processing**: Confidence thresholding
7. **MIDI Output**: Note events to host

### Visualization Path
1. **Audio Samples**: Raw input to waveform buffer
2. **Time Windowing**: Extract display samples
3. **Vertex Generation**: Convert to OpenGL data
4. **VBO Update**: Double-buffered rendering
5. **Shader Pipeline**: GPU-accelerated drawing
6. **Display**: Real-time waveform visualization

### Control Path
1. **Host Parameters**: Bypass, sensitivity settings
2. **Plugin State**: Atomic flag updates
3. **GUI Interaction**: User input handling
4. **Visual Feedback**: Hit indicators, FPS display

## Performance Characteristics

### Real-time Constraints
- **Audio Thread Deadline**: 1-10ms (buffer size dependent)
- **Memory Allocation**: Zero in audio thread
- **Lock Usage**: None in audio path
- **SIMD Optimization**: 4x-8x speedup on decimation

### Throughput Metrics
- **Audio Processing**: >10x real-time performance
- **AI Inference**: 20ms frames at 50 FPS
- **GUI Rendering**: 60-120 FPS sustained
- **Memory Usage**: <50MB total footprint

### Latency Analysis
- **Algorithmic Delay**: 8 samples (0.5ms at 16kHz)
- **Buffer Latency**: Host-dependent (1-50ms)
- **Processing Latency**: <1ms additional
- **Total Latency**: Dominated by host buffer size

### CPU Usage
- **Audio Thread**: <5% single core
- **AI Thread**: Configurable (model dependent)
- **GUI Thread**: <2% single core
- **Total System**: <20% on modern CPUs

## Conclusion

The KhDetector architecture achieves real-time audio processing through careful separation of concerns, lock-free data structures, and SIMD optimization. The modular design enables easy extension and maintenance while maintaining professional audio performance standards.

Key architectural strengths:
- **Deterministic Performance**: Real-time guarantees
- **Scalable Processing**: Thread pool adaptation
- **Visual Feedback**: High-performance OpenGL rendering
- **Cross-platform**: Consistent behavior across systems
- **Extensible Design**: Plugin-ready AI integration

This architecture serves as a foundation for professional audio plugin development with machine learning integration. 