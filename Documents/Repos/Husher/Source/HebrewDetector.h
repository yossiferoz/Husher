#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include "ONNXInterface.h"

class HebrewDetector
{
public:
    HebrewDetector();
    ~HebrewDetector();
    
    void prepare(double sampleRate, int blockSize);
    void reset();
    
    float processAudio(const juce::AudioBuffer<float>& buffer, float sensitivity);
    
private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    std::unique_ptr<ONNXInterface> modelInterface;
    
    std::vector<float> convertAudioToFeatures(const juce::AudioBuffer<float>& buffer);
    float applyPostProcessing(float rawConfidence, float sensitivity);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HebrewDetector)
};