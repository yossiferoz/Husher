#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

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
    
    // Temporary placeholder for actual detection logic
    float simulateDetection(const juce::AudioBuffer<float>& buffer, float sensitivity);
    
    // Future: ONNX model interface will go here
    // std::unique_ptr<ONNXInterface> modelInterface;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HebrewDetector)
};