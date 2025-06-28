#include "PluginProcessor.h"
#include "PluginEditor.h"

HusherAudioProcessorEditor::HusherAudioProcessorEditor (HusherAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);
    
    // Sensitivity slider setup
    sensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sensitivitySlider.setRange(0.0, 1.0, 0.01);
    sensitivitySlider.setValue(audioProcessor.getSensitivity());
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    sensitivitySlider.onValueChange = [this]() {
        // This will be connected to parameter when we add parameter attachment
    };
    addAndMakeVisible(sensitivitySlider);
    
    sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.attachToComponent(&sensitivitySlider, true);
    addAndMakeVisible(sensitivityLabel);
    
    // Start timer for GUI updates
    startTimerHz(30); // 30 FPS updates
}

HusherAudioProcessorEditor::~HusherAudioProcessorEditor()
{
    stopTimer();
}

void HusherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    
    // Draw title
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("Husher", getLocalBounds().removeFromTop(60), 
                      juce::Justification::centred, 1);
    
    // Draw subtitle
    g.setFont (14.0f);
    g.setColour (juce::Colours::lightgrey);
    g.drawFittedText ("Hebrew ח Detection", getLocalBounds().removeFromTop(80).removeFromBottom(20), 
                      juce::Justification::centred, 1);
    
    // Draw confidence meter
    g.setColour (juce::Colours::darkgrey);
    g.fillRect (confidenceMeterArea);
    
    // Draw confidence level
    auto confidenceWidth = static_cast<int>(currentConfidence * confidenceMeterArea.getWidth());
    auto confidenceRect = confidenceMeterArea.withWidth(confidenceWidth);
    
    // Color based on confidence level
    if (currentConfidence > 0.7f)
        g.setColour (juce::Colours::green);
    else if (currentConfidence > 0.4f)
        g.setColour (juce::Colours::yellow);
    else
        g.setColour (juce::Colours::red);
        
    g.fillRect (confidenceRect);
    
    // Draw confidence meter border
    g.setColour (juce::Colours::white);
    g.drawRect (confidenceMeterArea, 2);
    
    // Draw confidence text
    g.setFont (16.0f);
    g.setColour (juce::Colours::white);
    auto confidenceText = "Confidence: " + juce::String(currentConfidence * 100.0f, 1) + "%";
    g.drawText (confidenceText, confidenceMeterArea.withY(confidenceMeterArea.getBottom() + 10),
                juce::Justification::centred, true);
}

void HusherAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(100); // Space for title
    
    // Confidence meter
    confidenceMeterArea = area.removeFromTop(40).reduced(20);
    area.removeFromTop(40); // Space for confidence text
    
    // Sensitivity slider
    auto sliderArea = area.removeFromTop(40).reduced(20);
    sliderArea.removeFromLeft(80); // Space for label
    sensitivitySlider.setBounds(sliderArea);
}

void HusherAudioProcessorEditor::timerCallback()
{
    // Update confidence meter
    auto newConfidence = audioProcessor.getConfidenceLevel();
    if (std::abs(newConfidence - currentConfidence) > 0.01f)
    {
        currentConfidence = newConfidence;
        repaint();
    }
}