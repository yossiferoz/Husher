#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "WaveformVisualization.h"

class HusherAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                    private juce::Timer,
                                    private juce::Button::Listener
{
public:
    HusherAudioProcessorEditor (HusherAudioProcessor&);
    ~HusherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;

private:
    void timerCallback() override;
    void toggleWaveformView();
    void updateWaveformDisplay();
    
    HusherAudioProcessor& audioProcessor;
    
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    
    juce::TextButton hushItButton;
    
    juce::Rectangle<int> confidenceMeterArea;
    float currentConfidence = 0.0f;
    
    // Waveform visualization
    std::unique_ptr<WaveformVisualization> waveformDisplay;
    bool showingWaveform = false;
    int compactHeight = 300;
    int expandedHeight = 600;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HusherAudioProcessorEditor)
};