#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class HusherAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    HusherAudioProcessorEditor (HusherAudioProcessor&);
    ~HusherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    
    HusherAudioProcessor& audioProcessor;
    
    juce::Slider sensitivitySlider;
    juce::Label sensitivityLabel;
    
    juce::Rectangle<int> confidenceMeterArea;
    float currentConfidence = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HusherAudioProcessorEditor)
};