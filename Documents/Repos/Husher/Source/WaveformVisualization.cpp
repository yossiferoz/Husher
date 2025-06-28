#include "WaveformVisualization.h"

WaveformVisualization::WaveformVisualization()
{
    startTimerHz(30); // 30 FPS updates
}

WaveformVisualization::~WaveformVisualization()
{
    stopTimer();
}

void WaveformVisualization::setAudioBuffer(const AudioRecordingBuffer* buffer)
{
    audioBuffer = buffer;
    needsWaveformUpdate = true;
}

void WaveformVisualization::setDetections(const std::vector<HetDetection>& detections)
{
    currentDetections = detections;
    repaint();
}

void WaveformVisualization::setPlaybackPosition(double timeInSeconds)
{
    if (std::abs(playbackPosition - timeInSeconds) > 0.01) // Avoid unnecessary repaints
    {
        playbackPosition = timeInSeconds;
        repaint();
    }
}

void WaveformVisualization::paint(juce::Graphics& g)
{
    // Clear background
    g.fillAll(juce::Colours::black);
    
    if (!audioBuffer)
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("Click 'Hush It!' to capture and analyze audio", 
                   getLocalBounds(), juce::Justification::centred);
        return;
    }
    
    // Draw waveform
    drawWaveform(g);
    
    // Draw detection markers
    drawDetectionMarkers(g);
    
    // Draw playback cursor
    drawPlaybackCursor(g);
    
    // Draw border
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
}

void WaveformVisualization::resized()
{
    needsWaveformUpdate = true;
}

void WaveformVisualization::timerCallback()
{
    if (needsWaveformUpdate)
    {
        updateWaveformData();
        needsWaveformUpdate = false;
        repaint();
    }
}

void WaveformVisualization::updateWaveformData()
{
    if (!audioBuffer)
        return;
    
    const auto& recordedAudio = audioBuffer->getRecordedAudio();
    int numSamples = audioBuffer->getRecordingSamples();
    
    if (numSamples == 0)
        return;
    
    // Calculate samples per pixel
    int samplesPerPoint = std::max(1, numSamples / waveformResolution);
    int actualPoints = (numSamples + samplesPerPoint - 1) / samplesPerPoint;
    
    waveformMinValues.clear();
    waveformMaxValues.clear();
    waveformMinValues.reserve(actualPoints);
    waveformMaxValues.reserve(actualPoints);
    
    // Calculate min/max values for each visualization point
    for (int point = 0; point < actualPoints; ++point)
    {
        int startSample = point * samplesPerPoint;
        int endSample = std::min(startSample + samplesPerPoint, numSamples);
        
        float minVal = 0.0f;
        float maxVal = 0.0f;
        
        // Analyze all channels (mix to mono for visualization)
        for (int sample = startSample; sample < endSample; ++sample)
        {
            float mixedSample = 0.0f;
            int numChannels = recordedAudio.getNumChannels();
            
            for (int channel = 0; channel < numChannels; ++channel)
            {
                mixedSample += recordedAudio.getSample(channel, sample);
            }
            
            if (numChannels > 0)
                mixedSample /= numChannels;
            
            minVal = std::min(minVal, mixedSample);
            maxVal = std::max(maxVal, mixedSample);
        }
        
        waveformMinValues.push_back(minVal);
        waveformMaxValues.push_back(maxVal);
    }
}

void WaveformVisualization::drawWaveform(juce::Graphics& g)
{
    if (waveformMinValues.empty() || waveformMaxValues.empty())
        return;
    
    auto bounds = getLocalBounds().reduced(2);
    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    float centerY = bounds.getCentreY();
    
    g.setColour(waveformColour);
    
    juce::Path waveformPath;
    bool firstPoint = true;
    
    // Draw waveform as connected lines
    for (size_t i = 0; i < waveformMinValues.size(); ++i)
    {
        float x = bounds.getX() + (static_cast<float>(i) / static_cast<float>(waveformMinValues.size() - 1)) * width;
        float minY = centerY - (waveformMinValues[i] * height * 0.4f);
        float maxY = centerY - (waveformMaxValues[i] * height * 0.4f);
        
        if (firstPoint)
        {
            waveformPath.startNewSubPath(x, maxY);
            firstPoint = false;
        }
        else
        {
            waveformPath.lineTo(x, maxY);
        }
    }
    
    // Draw the path back for the minimum values
    for (int i = static_cast<int>(waveformMinValues.size()) - 1; i >= 0; --i)
    {
        float x = bounds.getX() + (static_cast<float>(i) / static_cast<float>(waveformMinValues.size() - 1)) * width;
        float minY = centerY - (waveformMinValues[i] * height * 0.4f);
        waveformPath.lineTo(x, minY);
    }
    
    waveformPath.closeSubPath();
    g.fillPath(waveformPath);
}

void WaveformVisualization::drawDetectionMarkers(juce::Graphics& g)
{
    if (!audioBuffer || currentDetections.empty())
        return;
    
    auto bounds = getLocalBounds().reduced(2);
    double totalLength = audioBuffer->getRecordingLength();
    
    if (totalLength <= 0.0)
        return;
    
    g.setColour(detectionColour);
    
    // Draw detection markers as vertical lines
    for (const auto& detection : currentDetections)
    {
        if (detection.timeStamp >= 0.0 && detection.timeStamp <= totalLength)
        {
            float x = bounds.getX() + static_cast<float>(detection.timeStamp / totalLength) * bounds.getWidth();
            
            // Line height based on confidence
            float lineHeight = bounds.getHeight() * (0.5f + detection.confidence * 0.5f);
            float startY = bounds.getCentreY() - lineHeight * 0.5f;
            float endY = bounds.getCentreY() + lineHeight * 0.5f;
            
            // Draw thick red line for detection
            g.drawLine(x, startY, x, endY, 3.0f);
            
            // Draw confidence text above the line
            if (bounds.getWidth() > 200) // Only show text if there's enough space
            {
                g.setFont(10.0f);
                auto confidenceText = juce::String(static_cast<int>(detection.confidence * 100)) + "%";
                auto textBounds = juce::Rectangle<float>(x - 15, startY - 15, 30, 12);
                g.drawText(confidenceText, textBounds.toNearestInt(), juce::Justification::centred);
            }
        }
    }
}

void WaveformVisualization::drawPlaybackCursor(juce::Graphics& g)
{
    if (!audioBuffer)
        return;
    
    double totalLength = audioBuffer->getRecordingLength();
    if (totalLength <= 0.0 || playbackPosition < 0.0 || playbackPosition > totalLength)
        return;
    
    auto bounds = getLocalBounds().reduced(2);
    float x = bounds.getX() + static_cast<float>(playbackPosition / totalLength) * bounds.getWidth();
    
    g.setColour(playbackCursorColour);
    g.drawLine(x, static_cast<float>(bounds.getY()), x, static_cast<float>(bounds.getBottom()), 2.0f);
}