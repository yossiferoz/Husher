#pragma once

#define VST3_SUPPORT  // Enable VST3 MIDI functionality

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "RingBuffer.h"
#include "PolyphaseDecimator.h"
#include "RealtimeThreadPool.h"
#include "AiInference.h"
#include "MidiEventHandler.h"
#include "WaveformData.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
class KhDetectorProcessor : public AudioEffect
{
public:
    KhDetectorProcessor();
    ~KhDetectorProcessor() override;

    // Create function
    static FUnknown* createInstance(void* /*context*/)
    {
        return (IAudioProcessor*)new KhDetectorProcessor;
    }

    // IComponent
    tresult PLUGIN_API initialize(FUnknown* context) override;
    tresult PLUGIN_API terminate() override;
    tresult PLUGIN_API setActive(TBool state) override;
    tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup) override;
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) override;

    // IAudioProcessor
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                         SpeakerArrangement* outputs, int32 numOuts) override;
    tresult PLUGIN_API process(ProcessData& data) override;
    uint32 PLUGIN_API getTailSamples() override { return kNoTail; }

    // IEditController
    tresult PLUGIN_API setState(IBStream* state) override;
    tresult PLUGIN_API getState(IBStream* state) override;
    
    /**
     * @brief Get reference to hit state for GUI visualization
     */
    std::atomic<bool>& getHitStateReference() { return mHadHit; }
    
    /**
     * @brief Get waveform buffer for GUI visualization
     */
    std::shared_ptr<KhDetector::WaveformBuffer4K> getWaveformBuffer() { return mWaveformBuffer; }

protected:
    // Processing
    template<typename SampleType>
    void processAudio(SampleType** inputs, SampleType** outputs, int32 numChannels, int32 sampleFrames);

private:
    // Parameters
    enum Parameters
    {
        kBypass = 0,
        kHitDetected,
        kNumParameters
    };

    // Audio processing constants
    static constexpr int kTargetSampleRate = 16000;
    static constexpr int kFrameSizeMs = 20;
    static constexpr int kFrameSize = (kTargetSampleRate * kFrameSizeMs) / 1000; // 320 samples at 16kHz
    static constexpr size_t kRingBufferSize = 2048; // Must be power of 2, larger than frame size

    // State
    bool mBypass = false;
    double mCurrentSampleRate = 48000.0;
    std::atomic<bool> mHadHit{false};  // Exposed to GUI/controller
    
    // Audio processing components
    KhDetector::PolyphaseDecimator<DECIM_FACTOR> mDecimator;
    KhDetector::RingBuffer<float, kRingBufferSize> mDecimatedBuffer;
    
    // AI processing components
    std::unique_ptr<KhDetector::AiInference> mAiInference;
    std::unique_ptr<KhDetector::RealtimeThreadPool> mThreadPool;
    
    // MIDI event handling
    std::unique_ptr<KhDetector::MidiEventHandler> mMidiHandler;
    int32_t mCurrentSamplePosition = 0;  // Track sample position for MIDI timing
    
    // Waveform visualization
    std::shared_ptr<KhDetector::WaveformBuffer4K> mWaveformBuffer;
    std::unique_ptr<KhDetector::SpectralAnalyzer> mSpectralAnalyzer;
    
    // Working buffers
    std::vector<float> mDecimatedSamples;
    std::vector<float> mProcessingFrame;
    
    // Frame processing
    void processDecimatedFrame(const float* frameData, int frameSize);
    void initializeForSampleRate(double sampleRate);
}; 