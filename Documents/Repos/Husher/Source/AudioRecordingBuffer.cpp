#include "AudioRecordingBuffer.h"
#include <algorithm>

AudioRecordingBuffer::AudioRecordingBuffer()
{
}

AudioRecordingBuffer::~AudioRecordingBuffer()
{
}

void AudioRecordingBuffer::prepare(double sampleRate, int maxLengthSeconds)
{
    currentSampleRate = sampleRate;
    maxSamples = static_cast<int>(sampleRate * maxLengthSeconds);
    
    std::lock_guard<std::mutex> lock(bufferMutex);
    recordedBuffer.setSize(2, maxSamples); // Stereo recording
    recordedBuffer.clear();
    
    reset();
}

void AudioRecordingBuffer::reset()
{
    recording.store(false);
    currentPosition.store(0);
    recordingStartTime.store(0.0);
    
    std::lock_guard<std::mutex> detectionsLock(detectionsMutex);
    detections.clear();
}

void AudioRecordingBuffer::startRecording()
{
    if (!recording.load())
    {
        reset();
        recordingStartTime.store(juce::Time::getMillisecondCounterHiRes() / 1000.0);
        recording.store(true);
    }
}

void AudioRecordingBuffer::stopRecording()
{
    recording.store(false);
}

void AudioRecordingBuffer::addAudioData(const juce::AudioBuffer<float>& buffer)
{
    if (!recording.load())
        return;
    
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    int position = currentPosition.load();
    int samplesToAdd = buffer.getNumSamples();
    int availableSpace = maxSamples - position;
    
    if (availableSpace <= 0)
    {
        // Recording buffer is full - stop recording
        recording.store(false);
        return;
    }
    
    // Limit samples to available space
    samplesToAdd = std::min(samplesToAdd, availableSpace);
    
    // Copy audio data
    for (int channel = 0; channel < std::min(buffer.getNumChannels(), recordedBuffer.getNumChannels()); ++channel)
    {
        recordedBuffer.copyFrom(channel, position, buffer, channel, 0, samplesToAdd);
    }
    
    // If input is mono but we have stereo buffer, copy to both channels
    if (buffer.getNumChannels() == 1 && recordedBuffer.getNumChannels() == 2)
    {
        recordedBuffer.copyFrom(1, position, buffer, 0, 0, samplesToAdd);
    }
    
    currentPosition.store(position + samplesToAdd);
}

void AudioRecordingBuffer::addDetection(double timeStamp, float confidence)
{
    if (!recording.load())
        return;
    
    std::lock_guard<std::mutex> lock(detectionsMutex);
    
    HetDetection detection;
    detection.timeStamp = timeStamp - recordingStartTime.load();
    detection.confidence = confidence;
    detection.samplePosition = static_cast<int>(detection.timeStamp * currentSampleRate);
    
    // Only add if timestamp is positive (after recording started)
    if (detection.timeStamp >= 0.0)
    {
        detections.push_back(detection);
    }
}

double AudioRecordingBuffer::getRecordingLength() const
{
    return currentPosition.load() / currentSampleRate;
}

void AudioRecordingBuffer::clearRecording()
{
    std::lock_guard<std::mutex> bufferLock(bufferMutex);
    std::lock_guard<std::mutex> detectionsLock(detectionsMutex);
    
    recordedBuffer.clear();
    detections.clear();
    currentPosition.store(0);
    recording.store(false);
}