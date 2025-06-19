#include "KhDetectorProcessor.h"
#include "KhDetectorController.h"
#include "KhDetectorVersion.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cmath>
#include <vector>

using namespace Steinberg;

//------------------------------------------------------------------------
KhDetectorProcessor::KhDetectorProcessor()
    : mDecimatedSamples(1024)  // Initial size, will be resized as needed
    , mProcessingFrame(kFrameSize)
{
    // Register its editor class (the same as used in vstgui4)
    setControllerClass(kKhDetectorControllerUID);
    
    // Initialize AI inference engine
    auto aiConfig = KhDetector::createDefaultModelConfig();
    aiConfig.inputSize = kFrameSize;  // 20ms frames at 16kHz
    mAiInference = KhDetector::createAiInference(aiConfig);
    
    // Initialize thread pool with low priority (below audio thread)
    mThreadPool = std::make_unique<KhDetector::RealtimeThreadPool>(
        KhDetector::RealtimeThreadPool::Priority::Low, 
        kFrameSize
    );
    
    // Initialize MIDI event handler
    KhDetector::MidiEventHandler::Config midiConfig;
    midiConfig.hitNote = 45;        // A2
    midiConfig.hitVelocity = 127;   // Maximum velocity
    midiConfig.channel = 0;         // MIDI channel 1 (0-based)
    midiConfig.sendNoteOff = true;  // Send note off when hit ends
    midiConfig.noteOffDelay = 0;    // Immediate note off
    mMidiHandler = std::make_unique<KhDetector::MidiEventHandler>(midiConfig);
    
    // Initialize waveform visualization
    mWaveformBuffer = std::make_shared<KhDetector::WaveformBuffer4K>();
    mSpectralAnalyzer = std::make_unique<KhDetector::SpectralAnalyzer>(kFrameSize);
    mSpectralAnalyzer->setSampleRate(kTargetSampleRate);
}

//------------------------------------------------------------------------
KhDetectorProcessor::~KhDetectorProcessor()
{
    // Stop thread pool before destroying other components
    if (mThreadPool) {
        mThreadPool->stop();
    }
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
    {
        return result;
    }

    // Create audio inputs/outputs
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    
    // Add MIDI event output bus
    addEventOutput(STR16("MIDI Out"));

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::terminate()
{
    return AudioEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::setActive(TBool state)
{
    if (state)
    {
        // Plugin is being activated - start thread pool
        if (mThreadPool && mAiInference)
        {
            mThreadPool->start(&mDecimatedBuffer, mAiInference.get(), kFrameSizeMs);
        }
        
        // Reset MIDI handler state
        if (mMidiHandler)
        {
            mMidiHandler->reset();
        }
        
        // Reset sample position counter
        mCurrentSamplePosition = 0;
    }
    else
    {
        // Plugin is being deactivated - stop thread pool  
        if (mThreadPool)
        {
            mThreadPool->stop();
        }
        
        // Reset MIDI handler to send any pending note offs
        if (mMidiHandler)
        {
            mMidiHandler->reset();
        }
    }
    
    return AudioEffect::setActive(state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::setupProcessing(ProcessSetup& newSetup)
{
    // Initialize for new sample rate
    mCurrentSampleRate = newSetup.sampleRate;
    initializeForSampleRate(mCurrentSampleRate);
    
    return AudioEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
        return kResultTrue;

    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                          SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns == 1 && numOuts == 1)
    {
        // The host wants Mono => Mono (or 1 channel -> 1 channel)
        if (SpeakerArr::getChannelCount(inputs[0]) == 1 && SpeakerArr::getChannelCount(outputs[0]) == 1)
        {
            return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        }
        // The host wants something else than Mono => Mono, in this case we are always Stereo => Stereo
        else
        {
            inputs[0] = outputs[0] = SpeakerArr::kStereo;
            return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        }
    }
    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::process(ProcessData& data)
{
    // Read inputs parameter changes
    if (data.inputParameterChanges)
    {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
        for (int32 index = 0; index < numParamsChanged; index++)
        {
            if (auto* paramQueue = data.inputParameterChanges->getParameterData(index))
            {
                Vst::ParamValue value;
                int32 sampleOffset;
                int32 numPoints = paramQueue->getPointCount();
                switch (paramQueue->getParameterId())
                {
                    case kBypass:
                        if (paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                            mBypass = (value > 0.5f);
                        break;
                }
            }
        }
    }

    // Process
    if (data.numInputs == 0 || data.numOutputs == 0)
    {
        return kResultOk;
    }

    // (simplification) we suppose in this example that we have the same input channel count than the output
    int32 numChannels = data.inputs[0].numChannels;

    // Get audio buffers
    uint32 sampleFramesSize = getSampleFramesSizeInBytes(processSetup, data.numSamples);
    void** in = getChannelBuffersPointer(processSetup, data.inputs[0]);
    void** out = getChannelBuffersPointer(processSetup, data.outputs[0]);

    if (mBypass)
    {
        // Silence outputs if bypassed
        for (int32 i = 0; i < numChannels; i++)
        {
            memset(out[i], 0, sampleFramesSize);
        }
    }
    else
    {
        // Process audio through decimation pipeline
        if (numChannels >= 2)
        {
            // Process stereo input
            float* leftChannel = (float*)in[0];
            float* rightChannel = (float*)in[1];
            
            // Ensure we have enough space for decimated samples
            int maxDecimatedSamples = (data.numSamples / DECIM_FACTOR) + 1;
            if (mDecimatedSamples.size() < maxDecimatedSamples)
            {
                mDecimatedSamples.resize(maxDecimatedSamples);
            }
            
            // Decimate stereo to mono
            int decimatedCount = mDecimator.processStereoToMono(
                leftChannel, rightChannel, 
                mDecimatedSamples.data(), 
                data.numSamples
            );
            
            // Enqueue decimated samples into ring buffer
            // The thread pool will consume these samples asynchronously
            for (int i = 0; i < decimatedCount; ++i)
            {
                float sample = mDecimatedSamples[i];
                
                if (!mDecimatedBuffer.push(sample))
                {
                    // Buffer overflow - this shouldn't happen with proper sizing
                    // but we handle it gracefully by dropping the sample
                }
                
                // Create waveform sample for visualization
                if (mWaveformBuffer) {
                    // Calculate additional features for the sample
                    float rms = std::abs(sample); // Simple RMS approximation
                    float zcr = 0.0f; // Zero crossing rate (would need history for accurate calculation)
                    float spectralCentroid = 0.0f; // Would need spectral analysis
                    
                    // Check if this is a hit sample
                    bool isHit = mHadHit.load();
                    
                    KhDetector::WaveformSample waveformSample(sample, rms, spectralCentroid, zcr, isHit);
                    mWaveformBuffer->push(waveformSample);
                }
            }
        }
        
        // Copy input to output (pass-through for now)
        if (in != out)
        {
            for (int32 i = 0; i < numChannels; i++)
            {
                if (in[i] != out[i])
                {
                    memcpy(out[i], in[i], sampleFramesSize);
                }
            }
        }
    }

    // Synchronize hit state with AI inference (non-blocking check)
    bool currentHit = false;
    if (mAiInference) {
        currentHit = mAiInference->hasHit();
        mHadHit.store(currentHit);
    }
    
    // Generate MIDI events for hit state changes
    if (mMidiHandler && data.outputEvents) {
        // Get host timestamp
        uint64_t hostTimeStamp = 0;
        if (data.processContext && data.processContext->state & ProcessContext::kSystemTimeValid) {
            hostTimeStamp = data.processContext->systemTime;
        }
        
        // Process hit state change and get MIDI event
        auto* midiEvent = mMidiHandler->processHitState(currentHit, 0, hostTimeStamp);
        if (midiEvent) {
#ifdef VST3_SUPPORT
            KhDetector::MidiEventHandler::sendVST3Event(data.outputEvents, *midiEvent);
#endif
        }
        
        // Check for pending note off events
        auto* pendingEvent = mMidiHandler->processPendingEvents(mCurrentSamplePosition);
        if (pendingEvent) {
#ifdef VST3_SUPPORT
            KhDetector::MidiEventHandler::sendVST3Event(data.outputEvents, *pendingEvent);
#endif
        }
        
        // Update sample position for timing
        mCurrentSamplePosition += data.numSamples;
    }
    
    // Output parameter changes to inform the host/GUI about hit state
    if (data.outputParameterChanges) {
        int32 index = 0;
        IParamValueQueue* paramQueue = data.outputParameterChanges->addParameterData(kHitDetected, index);
        if (paramQueue) {
            paramQueue->addPoint(0, mHadHit.load() ? 1.0 : 0.0, index);
        }
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;

    // Called when we load a preset, the model has to be reloaded
    IBStreamer streamer(state, kLittleEndian);
    
    int32 savedBypass = 0;
    if (streamer.readInt32(savedBypass) == false)
        return kResultFalse;
    mBypass = savedBypass > 0;

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API KhDetectorProcessor::getState(IBStream* state)
{
    // Here we need to save the model
    IBStreamer streamer(state, kLittleEndian);

    int32 toSaveBypass = mBypass ? 1 : 0;
    streamer.writeInt32(toSaveBypass);

    return kResultOk;
}

//------------------------------------------------------------------------
template<typename SampleType>
void KhDetectorProcessor::processAudio(SampleType** inputs, SampleType** outputs, int32 numChannels, int32 sampleFrames)
{
    // Simple pass-through for now
    for (int32 i = 0; i < numChannels; i++)
    {
        for (int32 j = 0; j < sampleFrames; j++)
        {
            outputs[i][j] = inputs[i][j];
        }
    }
}

//------------------------------------------------------------------------
void KhDetectorProcessor::initializeForSampleRate(double sampleRate)
{
    mCurrentSampleRate = sampleRate;
    
    // Reset decimator for new sample rate
    mDecimator.reset();
    
    // Clear ring buffer
    mDecimatedBuffer.clear();
    
    // Calculate expected decimation factor based on sample rate
    double expectedDecimationFactor = sampleRate / kTargetSampleRate;
    
    // For now, we handle 48kHz (factor=3) optimally
    // TODO: Add support for 44.1kHz with fractional decimation or cascade of decimators
    if (std::abs(expectedDecimationFactor - DECIM_FACTOR) > 0.1)
    {
        // Log warning about non-optimal decimation factor
        // In a real implementation, you might want to adjust the filter or use a different approach
    }
}

//------------------------------------------------------------------------
void KhDetectorProcessor::processDecimatedFrame(const float* frameData, int frameSize)
{
    // This is where you would implement your actual audio analysis/detection algorithm
    // For now, this is a placeholder that demonstrates the frame processing structure
    
    if (frameSize != kFrameSize)
    {
        return; // Invalid frame size
    }
    
    // Example processing: compute RMS energy of the frame
    float energy = 0.0f;
    for (int i = 0; i < frameSize; ++i)
    {
        energy += frameData[i] * frameData[i];
    }
    energy = std::sqrt(energy / frameSize);
    
    // TODO: Implement your detection algorithm here
    // This could include:
    // - Feature extraction (spectral features, MFCCs, etc.)
    // - Machine learning inference
    // - Pattern matching
    // - Onset detection
    // - Etc.
    
    // For demonstration, we'll just track the energy level
    // In a real implementation, you might send this data to the GUI thread
    // or trigger parameter changes based on the analysis
    
    (void)energy; // Suppress unused variable warning
} 