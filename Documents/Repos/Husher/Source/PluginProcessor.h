#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "HebrewDetector.h"
#include "AudioRecordingBuffer.h"

class HusherAudioProcessor : public juce::AudioProcessor
{
public:
    HusherAudioProcessor();
    ~HusherAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Plugin-specific methods
    float getConfidenceLevel() const { return confidenceLevel.load(); }
    float getSensitivity() const { return *sensitivityParam; }
    
    // Recording functionality
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recordingBuffer.isRecording(); }
    const AudioRecordingBuffer* getRecordingBuffer() const { return &recordingBuffer; }

private:
    juce::AudioParameterFloat* sensitivityParam;
    std::atomic<float> confidenceLevel{0.0f};
    
    HebrewDetector detector;
    AudioRecordingBuffer recordingBuffer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HusherAudioProcessor)
};