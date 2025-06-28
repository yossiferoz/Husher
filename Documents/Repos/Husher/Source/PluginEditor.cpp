#include "PluginProcessor.h"
#include "PluginEditor.h"

HusherAudioProcessorEditor::HusherAudioProcessorEditor (HusherAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, compactHeight);
    setResizable(true, true);
    setResizeLimits(300, 200, 1000, 800);
    
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
    
    // "Hush It!" button setup
    hushItButton.setButtonText("Hush It!");
    hushItButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    hushItButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    hushItButton.addListener(this);
    addAndMakeVisible(hushItButton);
    
    // Initialize waveform display (but don't make visible yet)
    waveformDisplay = std::make_unique<WaveformVisualization>();
    addChildComponent(waveformDisplay.get());
    
    // Initialize confidence meter area (temporary until resized() is called)
    confidenceMeterArea = juce::Rectangle<int>(20, 100, 360, 40);
    
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
    g.drawFittedText ("Husher", juce::Rectangle<int>(0, 10, getWidth(), 30), 
                      juce::Justification::centred, 1);
    
    // Draw subtitle
    g.setFont (14.0f);
    g.setColour (juce::Colours::lightgrey);
    g.drawFittedText (juce::String::fromUTF8("Hebrew \u05d7 Detection"), juce::Rectangle<int>(0, 45, getWidth(), 20), 
                      juce::Justification::centred, 1);
    
    // Draw confidence meter background
    g.setColour (juce::Colours::darkgrey);
    g.fillRect (confidenceMeterArea);
    
    // Draw confidence level bar
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
    
    // Draw confidence text below meter
    g.setFont (12.0f);
    g.setColour (juce::Colours::white);
    auto confidenceText = "Confidence: " + juce::String(currentConfidence * 100.0f, 1) + "%";
    auto textArea = juce::Rectangle<int>(confidenceMeterArea.getX(), confidenceMeterArea.getBottom() + 5, 
                                        confidenceMeterArea.getWidth(), 20);
    g.drawText (confidenceText, textArea, juce::Justification::centred, true);
}

void HusherAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(80); // Space for title and subtitle
    
    // Confidence meter area
    confidenceMeterArea = area.removeFromTop(50).reduced(20);
    area.removeFromTop(20); // Space for confidence text
    
    // Sensitivity slider area
    auto sliderArea = area.removeFromTop(50).reduced(20);
    sliderArea.removeFromLeft(80); // Space for label
    sensitivitySlider.setBounds(sliderArea);
    
    // "Hush It!" button area
    auto buttonArea = area.removeFromTop(50).reduced(20);
    hushItButton.setBounds(buttonArea);
    
    // Waveform display area (if visible)
    if (showingWaveform && waveformDisplay)
    {
        auto waveformArea = area.reduced(10);
        waveformDisplay->setBounds(waveformArea);
    }
    
    // Debug: Force repaint when resized
    repaint();
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
    
    // Update waveform display if visible
    if (showingWaveform)
    {
        updateWaveformDisplay();
    }
}

void HusherAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &hushItButton)
    {
        toggleWaveformView();
    }
}

void HusherAudioProcessorEditor::toggleWaveformView()
{
    if (!showingWaveform)
    {
        // Start recording and show waveform
        audioProcessor.startRecording();
        showingWaveform = true;
        waveformDisplay->setVisible(true);
        hushItButton.setButtonText("Stop Analysis");
        hushItButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);
        
        // Expand window
        setSize(getWidth(), expandedHeight);
    }
    else
    {
        // Stop recording and analyze
        audioProcessor.stopRecording();
        showingWaveform = false;
        hushItButton.setButtonText("Hush It!");
        hushItButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        
        // Update waveform with final data
        updateWaveformDisplay();
        
        // Keep waveform visible for analysis
        // (Don't hide it so user can see results)
    }
    
    resized(); // Trigger layout update
}

void HusherAudioProcessorEditor::updateWaveformDisplay()
{
    if (!waveformDisplay)
        return;
    
    // Get recording buffer and detections from processor
    auto* recordingBuffer = audioProcessor.getRecordingBuffer();
    if (recordingBuffer)
    {
        waveformDisplay->setAudioBuffer(recordingBuffer);
        waveformDisplay->setDetections(recordingBuffer->getDetections());
    }
}