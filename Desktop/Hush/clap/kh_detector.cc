#include <clap/clap.h>
#include <cstring>
#include <memory>
#include <atomic>
#include <vector>
#include <cmath>

// Include our core components
#include "../src/RingBuffer.h"
#include "../src/PolyphaseDecimator.h"
#include "../src/RealtimeThreadPool.h"
#include "../src/AiInference.h"
#include "../src/MidiEventHandler.h"
#include "../src/WaveformData.h"
#include "../src/PostProcessor.h"

namespace KhDetector {

// Plugin descriptor
static const clap_plugin_descriptor_t s_descriptor = {
    CLAP_VERSION_INIT,
    "com.khdetector.clap",
    "KhDetector CLAP",
    "KhDetector",
    "https://github.com/khdetector/khdetector",
    "",
    "",
    "1.0.0",
    "Audio analysis and hit detection plugin",
    CLAP_PLUGIN_AUDIO_EFFECT,
    nullptr
};

// Parameter IDs
enum ParameterIds {
    PARAM_BYPASS = 0,
    PARAM_HIT_DETECTED = 1,
    PARAM_SENSITIVITY = 2,
    PARAM_COUNT
};

// Parameter info
static const clap_param_info_t s_param_infos[PARAM_COUNT] = {
    {
        PARAM_BYPASS,
        CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_BYPASS,
        nullptr,
        "Bypass",
        "",
        0.0, 1.0, 0.0
    },
    {
        PARAM_HIT_DETECTED,
        CLAP_PARAM_IS_READONLY,
        nullptr,
        "Hit Detected",
        "",
        0.0, 1.0, 0.0
    },
    {
        PARAM_SENSITIVITY,
        0,
        nullptr,
        "Sensitivity",
        "",
        0.0, 1.0, 0.6
    }
};

class KhDetectorClapPlugin {
public:
    KhDetectorClapPlugin(const clap_host_t* host)
        : mHost(host)
        , mCurrentSamplePosition(0)
        , mBypass(false)
        , mSensitivity(0.6f)
        , mHadHit(false)
        , mDecimator()
        , mDecimatedBuffer()
        , mDecimatedSamples(kFrameSize)
        , mProcessingFrame(kFrameSize)
    {
        // Initialize waveform buffer
        mWaveformBuffer = std::make_shared<WaveformBuffer4K>();
        mSpectralAnalyzer = std::make_unique<SpectralAnalyzer>();
        
        // Initialize AI inference and thread pool
        mAiInference = std::make_unique<AiInference>();
        mThreadPool = std::make_unique<RealtimeThreadPool>(
            std::thread::hardware_concurrency() - 1,
            RealtimeThreadPool::Priority::High
        );
        
        // Initialize MIDI handler
        mMidiHandler = std::make_unique<MidiEventHandler>();
    }

    ~KhDetectorClapPlugin() = default;

    bool init() {
        return true;
    }

    void destroy() {
        if (mThreadPool) {
            mThreadPool->stop();
        }
    }

    bool activate(double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count) {
        mCurrentSampleRate = sample_rate;
        
        // Initialize for sample rate
        initializeForSampleRate(sample_rate);
        
        // Start thread pool
        if (mThreadPool) {
            mThreadPool->start(&mDecimatedBuffer, mAiInference.get());
        }
        
        return true;
    }

    void deactivate() {
        if (mThreadPool) {
            mThreadPool->stop();
        }
    }

    bool start_processing() {
        return true;
    }

    void stop_processing() {
        // Clear any pending processing
        mDecimatedBuffer.clear();
    }

    void reset() {
        mDecimatedBuffer.clear();
        mCurrentSamplePosition = 0;
        mHadHit.store(false);
        
        // Reset AI inference state
        mHadHit.store(false);
    }

    clap_process_status process(const clap_process_t* process) {
        // Handle parameter changes
        handleParameterChanges(process);
        
        // Handle input events
        handleInputEvents(process);
        
        // Process audio
        if (process->audio_inputs_count > 0 && process->audio_outputs_count > 0) {
            processAudio(process);
        }
        
        // Handle output events
        handleOutputEvents(process);
        
        return CLAP_PROCESS_CONTINUE;
    }

    // Extensions
    const void* get_extension(const char* id) {
        if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
            return &s_params_extension;
        }
        if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
            return &s_audio_ports_extension;
        }
        if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
            return &s_note_ports_extension;
        }
        if (strcmp(id, CLAP_EXT_STATE) == 0) {
            return &s_state_extension;
        }
        return nullptr;
    }

    // Parameter extension methods
    uint32_t params_count() const {
        return PARAM_COUNT;
    }

    bool params_get_info(uint32_t param_index, clap_param_info_t* param_info) const {
        if (param_index >= PARAM_COUNT) {
            return false;
        }
        *param_info = s_param_infos[param_index];
        return true;
    }

    bool params_get_value(clap_id param_id, double* value) const {
        switch (param_id) {
            case PARAM_BYPASS:
                *value = mBypass ? 1.0 : 0.0;
                return true;
            case PARAM_HIT_DETECTED:
                *value = mHadHit.load() ? 1.0 : 0.0;
                return true;
            case PARAM_SENSITIVITY:
                *value = static_cast<double>(mSensitivity);
                return true;
            default:
                return false;
        }
    }

    bool params_value_to_text(clap_id param_id, double value, char* display, uint32_t size) const {
        switch (param_id) {
            case PARAM_BYPASS:
                snprintf(display, size, "%s", value > 0.5 ? "On" : "Off");
                return true;
            case PARAM_HIT_DETECTED:
                snprintf(display, size, "%s", value > 0.5 ? "HIT" : "---");
                return true;
            case PARAM_SENSITIVITY:
                snprintf(display, size, "%.2f", value);
                return true;
            default:
                return false;
        }
    }

    bool params_text_to_value(clap_id param_id, const char* display, double* value) const {
        switch (param_id) {
            case PARAM_BYPASS:
                if (strcmp(display, "On") == 0) {
                    *value = 1.0;
                    return true;
                } else if (strcmp(display, "Off") == 0) {
                    *value = 0.0;
                    return true;
                }
                return false;
            case PARAM_SENSITIVITY:
                {
                    char* endptr;
                    double val = strtod(display, &endptr);
                    if (endptr != display && val >= 0.0 && val <= 1.0) {
                        *value = val;
                        return true;
                    }
                }
                return false;
            default:
                return false;
        }
    }

    void params_flush(const clap_input_events_t* in, const clap_output_events_t* out) {
        handleParameterEvents(in);
        
        // Output hit detection parameter changes
        if (mHadHit.exchange(false)) {
            clap_event_param_value_t param_event = {};
            param_event.header.size = sizeof(param_event);
            param_event.header.time = 0;
            param_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            param_event.header.type = CLAP_EVENT_PARAM_VALUE;
            param_event.header.flags = 0;
            param_event.param_id = PARAM_HIT_DETECTED;
            param_event.cookie = nullptr;
            param_event.note_id = -1;
            param_event.port_index = -1;
            param_event.channel = -1;
            param_event.key = -1;
            param_event.value = 1.0;
            
            out->try_push(out, &param_event.header);
        }
    }

    // Audio ports extension
    uint32_t audio_ports_count(bool is_input) const {
        return 1;
    }

    bool audio_ports_get(uint32_t index, bool is_input, clap_audio_port_info_t* info) const {
        if (index != 0) {
            return false;
        }
        
        info->id = 0;
        strcpy(info->name, is_input ? "Input" : "Output");
        info->channel_count = 2;
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type = CLAP_PORT_STEREO;
        info->in_place_pair = is_input ? CLAP_INVALID_ID : 0;
        
        return true;
    }

    // Note ports extension
    uint32_t note_ports_count(bool is_input) const {
        return is_input ? 0 : 1;
    }

    bool note_ports_get(uint32_t index, bool is_input, clap_note_port_info_t* info) const {
        if (is_input || index != 0) {
            return false;
        }
        
        info->id = 0;
        strcpy(info->name, "MIDI Output");
        info->supported_dialects = CLAP_NOTE_DIALECT_MIDI;
        info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;
        
        return true;
    }

    // State extension
    bool state_save(const clap_ostream_t* stream) const {
        // Simple state: bypass and sensitivity
        struct State {
            bool bypass;
            float sensitivity;
        } state = { mBypass, mSensitivity };
        
        return stream->write(stream, &state, sizeof(state)) == sizeof(state);
    }

    bool state_load(const clap_istream_t* stream) {
        struct State {
            bool bypass;
            float sensitivity;
        } state;
        
        if (stream->read(stream, &state, sizeof(state)) == sizeof(state)) {
            mBypass = state.bypass;
            mSensitivity = state.sensitivity;
            
            // Sensitivity updated (handled in processing)
            
            return true;
        }
        return false;
    }

private:
    // Audio processing constants
    static constexpr int kTargetSampleRate = 16000;
    static constexpr int kFrameSizeMs = 20;
    static constexpr int kFrameSize = (kTargetSampleRate * kFrameSizeMs) / 1000; // 320 samples at 16kHz
    static constexpr size_t kRingBufferSize = 2048; // Must be power of 2, larger than frame size

    // Host and state
    const clap_host_t* mHost;
    double mCurrentSampleRate = 48000.0;
    int32_t mCurrentSamplePosition = 0;
    bool mBypass;
    float mSensitivity;
    std::atomic<bool> mHadHit;

    // Audio processing components
    PolyphaseDecimator<DECIM_FACTOR> mDecimator;
    RingBuffer<float, kRingBufferSize> mDecimatedBuffer;
    
    // AI processing components
    std::unique_ptr<AiInference> mAiInference;
    std::unique_ptr<RealtimeThreadPool> mThreadPool;
    
    // MIDI event handling
    std::unique_ptr<MidiEventHandler> mMidiHandler;
    
    // Waveform visualization
    std::shared_ptr<WaveformBuffer4K> mWaveformBuffer;
    std::unique_ptr<SpectralAnalyzer> mSpectralAnalyzer;
    
    // Working buffers
    std::vector<float> mDecimatedSamples;
    std::vector<float> mProcessingFrame;

    void initializeForSampleRate(double sampleRate) {
        mCurrentSampleRate = sampleRate;
        
        // Calculate decimation factor based on sample rate
        int decimationFactor = static_cast<int>(std::round(sampleRate / kTargetSampleRate));
        decimationFactor = std::max(1, std::min(decimationFactor, 8)); // Clamp to reasonable range
        
        // Clear buffers
        mDecimatedBuffer.clear();
        mDecimatedSamples.clear();
        mProcessingFrame.clear();
        
        // Resize working buffers
        mDecimatedSamples.resize(kFrameSize);
        mProcessingFrame.resize(kFrameSize);
        
        // Initialize AI inference with default config
        if (mAiInference) {
            KhDetector::AiInference::ModelConfig config;
            config.inputSize = kFrameSize;
            mAiInference->initialize(config);
        }
    }

    void handleParameterChanges(const clap_process_t* process) {
        if (process->in_events) {
            handleParameterEvents(process->in_events);
        }
    }

    void handleParameterEvents(const clap_input_events_t* events) {
        uint32_t event_count = events->size(events);
        for (uint32_t i = 0; i < event_count; ++i) {
            const clap_event_header_t* header = events->get(events, i);
            if (header->space_id == CLAP_CORE_EVENT_SPACE_ID && 
                header->type == CLAP_EVENT_PARAM_VALUE) {
                
                const clap_event_param_value_t* param_event = 
                    reinterpret_cast<const clap_event_param_value_t*>(header);
                
                switch (param_event->param_id) {
                    case PARAM_BYPASS:
                        mBypass = param_event->value > 0.5;
                        break;
                    case PARAM_SENSITIVITY:
                        mSensitivity = static_cast<float>(param_event->value);
                        // Update sensitivity (handled by post-processor)
                        break;
                }
            }
        }
    }

    void handleInputEvents(const clap_process_t* process) {
        // Handle any additional input events (MIDI, etc.)
        // For now, we primarily handle parameter changes
    }

    void handleOutputEvents(const clap_process_t* process) {
        // Handle MIDI output events for hit detection
        if (process->out_events && mHadHit.exchange(false)) {
            // Create MIDI note on event
            clap_event_midi_t midi_event = {};
            midi_event.header.size = sizeof(midi_event);
            midi_event.header.time = mCurrentSamplePosition;
            midi_event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            midi_event.header.type = CLAP_EVENT_MIDI;
            midi_event.header.flags = 0;
            midi_event.port_index = 0;
            
            // MIDI Note On: note 45 (A2), velocity 127
            midi_event.data[0] = 0x90; // Note on, channel 0
            midi_event.data[1] = 45;   // Note number (A2)
            midi_event.data[2] = 127;  // Velocity
            
            process->out_events->try_push(process->out_events, &midi_event.header);
        }
    }

    void processAudio(const clap_process_t* process) {
        if (mBypass) {
            // Bypass: copy input to output
            const float* input = process->audio_inputs[0].data32[0];
            float* output = process->audio_outputs[0].data32[0];
            memcpy(output, input, process->frames_count * sizeof(float));
            
            if (process->audio_inputs[0].channel_count > 1 && 
                process->audio_outputs[0].channel_count > 1) {
                const float* input_r = process->audio_inputs[0].data32[1];
                float* output_r = process->audio_outputs[0].data32[1];
                memcpy(output_r, input_r, process->frames_count * sizeof(float));
            }
            return;
        }

        // Process audio with decimation and AI inference
        const float* input_l = process->audio_inputs[0].data32[0];
        const float* input_r = process->audio_inputs[0].channel_count > 1 ? 
                               process->audio_inputs[0].data32[1] : input_l;
        
        float* output_l = process->audio_outputs[0].data32[0];
        float* output_r = process->audio_outputs[0].channel_count > 1 ? 
                          process->audio_outputs[0].data32[1] : nullptr;

        // Copy input to output (pass-through)
        memcpy(output_l, input_l, process->frames_count * sizeof(float));
        if (output_r && input_r != input_l) {
            memcpy(output_r, input_r, process->frames_count * sizeof(float));
        }

        // Process audio for analysis
        processAudioAnalysis(input_l, input_r, process->frames_count);
        
        mCurrentSamplePosition += process->frames_count;
    }

    void processAudioAnalysis(const float* input_l, const float* input_r, uint32_t frame_count) {
        // Decimate stereo to mono at target sample rate
        for (uint32_t i = 0; i < frame_count; ++i) {
            float mono_sample = (input_l[i] + input_r[i]) * 0.5f;
            
            float decimated_sample;
            // Simple decimation by factor of 3 (placeholder)
            if ((i % DECIM_FACTOR) == 0) {
                decimated_sample = mono_sample;
                // Add to ring buffer
                if (!mDecimatedBuffer.push(decimated_sample)) {
                    // Buffer full, this shouldn't happen with proper sizing
                    continue;
                }
                
                // Check if we have enough samples for a frame
                if (mDecimatedBuffer.size() >= kFrameSize) {
                    // Extract frame for processing
                    size_t samples_popped = mDecimatedBuffer.pop_bulk(mProcessingFrame.data(), kFrameSize);
                    if (samples_popped == kFrameSize) {
                        processDecimatedFrame(mProcessingFrame.data(), kFrameSize);
                    }
                }
            }
        }
    }

    void processDecimatedFrame(const float* frameData, int frameSize) {
        // Add to waveform buffer for visualization
        if (mWaveformBuffer && mSpectralAnalyzer) {
            // Calculate audio features
            float rms = 0.0f;
            float zcr = 0.0f;
            for (int i = 0; i < frameSize; ++i) {
                rms += frameData[i] * frameData[i];
                if (i > 0 && (frameData[i] >= 0.0f) != (frameData[i-1] >= 0.0f)) {
                    zcr += 1.0f;
                }
            }
            rms = std::sqrt(rms / frameSize);
            zcr /= frameSize;
            
            // Perform spectral analysis
            auto spectral_frame = mSpectralAnalyzer->analyze(frameData, frameSize);
            
            // Create waveform sample
            WaveformSample sample;
            sample.amplitude = frameData[frameSize/2]; // Mid-frame amplitude
            sample.rms = rms;
            sample.spectralCentroid = spectral_frame.spectralCentroid;
            sample.zeroCrossingRate = zcr;
            sample.timestamp = std::chrono::steady_clock::now();
            sample.isHit = false; // Will be updated by AI inference
            
            // Add sample to waveform buffer (simplified)
            // mWaveformBuffer->addSample(sample);
        }
        
        // Submit frame for AI processing
        if (mThreadPool && mAiInference) {
            std::vector<float> frame_copy(frameData, frameData + frameSize);
            mThreadPool->submitTask([this, frame_copy = std::move(frame_copy)]() {
                // Simulate AI processing and hit detection
                float energy = 0.0f;
                for (const auto& sample : frame_copy) {
                    energy += sample * sample;
                }
                energy /= frame_copy.size();
                
                // Simple threshold-based hit detection
                if (energy > mSensitivity) {
                    mHadHit.store(true);
                }
            });
        }
    }

    // Static extension implementations
    static const clap_plugin_params_t s_params_extension;
    static const clap_plugin_audio_ports_t s_audio_ports_extension;
    static const clap_plugin_note_ports_t s_note_ports_extension;
    static const clap_plugin_state_t s_state_extension;
};

// Static extension implementations
const clap_plugin_params_t KhDetectorClapPlugin::s_params_extension = {
    [](const clap_plugin_t* plugin) -> uint32_t {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->params_count();
    },
    [](const clap_plugin_t* plugin, uint32_t param_index, clap_param_info_t* param_info) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->params_get_info(param_index, param_info);
    },
    [](const clap_plugin_t* plugin, clap_id param_id, double* value) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->params_get_value(param_id, value);
    },
    [](const clap_plugin_t* plugin, clap_id param_id, double value, char* display, uint32_t size) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->params_value_to_text(param_id, value, display, size);
    },
    [](const clap_plugin_t* plugin, clap_id param_id, const char* display, double* value) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->params_text_to_value(param_id, display, value);
    },
    [](const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
        static_cast<KhDetectorClapPlugin*>(plugin->plugin_data)->params_flush(in, out);
    }
};

const clap_plugin_audio_ports_t KhDetectorClapPlugin::s_audio_ports_extension = {
    [](const clap_plugin_t* plugin, bool is_input) -> uint32_t {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->audio_ports_count(is_input);
    },
    [](const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_audio_port_info_t* info) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->audio_ports_get(index, is_input, info);
    }
};

const clap_plugin_note_ports_t KhDetectorClapPlugin::s_note_ports_extension = {
    [](const clap_plugin_t* plugin, bool is_input) -> uint32_t {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->note_ports_count(is_input);
    },
    [](const clap_plugin_t* plugin, uint32_t index, bool is_input, clap_note_port_info_t* info) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->note_ports_get(index, is_input, info);
    }
};

const clap_plugin_state_t KhDetectorClapPlugin::s_state_extension = {
    [](const clap_plugin_t* plugin, const clap_ostream_t* stream) -> bool {
        return static_cast<const KhDetectorClapPlugin*>(plugin->plugin_data)->state_save(stream);
    },
    [](const clap_plugin_t* plugin, const clap_istream_t* stream) -> bool {
        return static_cast<KhDetectorClapPlugin*>(plugin->plugin_data)->state_load(stream);
    }
};

} // namespace KhDetector

// CLAP plugin interface implementation
static bool plugin_init(const clap_plugin_t* plugin) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->init();
}

static void plugin_destroy(const clap_plugin_t* plugin) {
    auto* p = static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data);
    p->destroy();
    delete p;
}

static bool plugin_activate(const clap_plugin_t* plugin, double sample_rate, uint32_t min_frames, uint32_t max_frames) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->activate(sample_rate, min_frames, max_frames);
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->deactivate();
}

static bool plugin_start_processing(const clap_plugin_t* plugin) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->start_processing();
}

static void plugin_stop_processing(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->stop_processing();
}

static void plugin_reset(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->reset();
}

static clap_process_status plugin_process(const clap_plugin_t* plugin, const clap_process_t* process) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->process(process);
}

static const void* plugin_get_extension(const clap_plugin_t* plugin, const char* id) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->get_extension(id);
}

static void plugin_on_main_thread(const clap_plugin_t* plugin) {
    // Handle main thread callbacks if needed
}

static const clap_plugin_t s_plugin_class = {
    &KhDetector::s_descriptor,
    nullptr, // plugin_data will be set per instance
    plugin_init,
    plugin_destroy,
    plugin_activate,
    plugin_deactivate,
    plugin_start_processing,
    plugin_stop_processing,
    plugin_reset,
    plugin_process,
    plugin_get_extension,
    plugin_on_main_thread
};

// Factory implementation
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(const clap_plugin_factory_t* factory, uint32_t index) {
    if (index == 0) {
        return &KhDetector::s_descriptor;
    }
    return nullptr;
}

static const clap_plugin_t* factory_create_plugin(const clap_plugin_factory_t* factory, const clap_host_t* host, const char* plugin_id) {
    if (strcmp(plugin_id, KhDetector::s_descriptor.id) == 0) {
        auto* plugin_instance = new KhDetector::KhDetectorClapPlugin(host);
        
        clap_plugin_t* plugin = new clap_plugin_t;
        *plugin = s_plugin_class;
        plugin->plugin_data = plugin_instance;
        
        return plugin;
    }
    return nullptr;
}

static const clap_plugin_factory_t s_plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    factory_create_plugin
};

// Entry point
extern "C" {

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    [](const char* plugin_path) -> bool {
        // Plugin initialization
        return true;
    },
    []() {
        // Plugin cleanup
    },
    [](const char* factory_id) -> const void* {
        if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
            return &s_plugin_factory;
        }
        return nullptr;
    }
};

} 