#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioRecordingBuffer.h"

class WaveformVisualization : public juce::Component,
                              private juce::Timer
{
public:
    WaveformVisualization();
    ~WaveformVisualization() override;
    
    void setAudioBuffer(const AudioRecordingBuffer* buffer);
    void setDetections(const std::vector<HetDetection>& detections);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Playback position (for future playback features)
    void setPlaybackPosition(double timeInSeconds);
    
private:
    void timerCallback() override;
    void updateWaveformData();
    void drawWaveform(juce::Graphics& g);
    void drawDetectionMarkers(juce::Graphics& g);
    void drawPlaybackCursor(juce::Graphics& g);
    
    const AudioRecordingBuffer* audioBuffer = nullptr;
    std::vector<HetDetection> currentDetections;
    
    // Waveform data for visualization
    std::vector<float> waveformMinValues;
    std::vector<float> waveformMaxValues;
    int waveformResolution = 1000; // Number of points to display
    
    double playbackPosition = 0.0; // Current playback position in seconds
    bool needsWaveformUpdate = true;
    
    // Visual settings
    juce::Colour waveformColour = juce::Colours::lightblue;
    juce::Colour detectionColour = juce::Colours::red;
    juce::Colour playbackCursorColour = juce::Colours::yellow;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformVisualization)
};