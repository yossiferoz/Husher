#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <atomic>
#include <mutex>

struct HetDetection
{
    double timeStamp;      // Time in seconds from start of recording
    float confidence;      // Detection confidence (0.0 - 1.0)
    int samplePosition;    // Sample position in the recording
};

class AudioRecordingBuffer
{
public:
    AudioRecordingBuffer();
    ~AudioRecordingBuffer();
    
    void prepare(double sampleRate, int maxLengthSeconds = 300); // 5 minutes max
    void reset();
    
    // Recording control
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recording.load(); }
    
    // Add audio data (called from audio thread)
    void addAudioData(const juce::AudioBuffer<float>& buffer);
    
    // Add detection result
    void addDetection(double timeStamp, float confidence);
    
    // Get recorded audio
    const juce::AudioBuffer<float>& getRecordedAudio() const { return recordedBuffer; }
    const std::vector<HetDetection>& getDetections() const { return detections; }
    
    // Get recording info
    double getRecordingLength() const; // in seconds
    int getRecordingSamples() const { return currentPosition.load(); }
    double getSampleRate() const { return currentSampleRate; }
    
    // Clear data
    void clearRecording();
    
private:
    juce::AudioBuffer<float> recordedBuffer;
    std::vector<HetDetection> detections;
    
    std::atomic<bool> recording{false};
    std::atomic<int> currentPosition{0};
    std::atomic<double> recordingStartTime{0.0};
    
    double currentSampleRate = 44100.0;
    int maxSamples = 0;
    
    mutable std::mutex detectionsMutex;
    mutable std::mutex bufferMutex;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecordingBuffer)
};