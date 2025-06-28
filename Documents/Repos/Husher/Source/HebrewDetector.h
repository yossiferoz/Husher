#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <atomic>
#include "RealtimeInferenceEngine.h"

class HebrewDetector
{
public:
    HebrewDetector();
    ~HebrewDetector();
    
    void prepare(double sampleRate, int blockSize);
    void reset();
    
    float processAudio(const juce::AudioBuffer<float>& buffer, float sensitivity);
    
    // Performance monitoring
    float getAverageLatency() const;
    bool isHealthy() const;
    
private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    std::unique_ptr<RealtimeInferenceEngine> inferenceEngine;
    
    // Cache for smooth confidence output
    std::atomic<float> lastConfidence{0.0f};
    std::atomic<float> smoothedConfidence{0.0f};
    
    // Audio processing
    std::vector<float> audioBuffer;
    static constexpr int PROCESSING_WINDOW_SIZE = 1024; // 25ms @ 44.1kHz
    
    std::vector<float> convertAudioToFeatures(const juce::AudioBuffer<float>& buffer);
    float applyPostProcessing(float rawConfidence, float sensitivity);
    void updateSmoothedConfidence(float newConfidence);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HebrewDetector)
};