#include "HebrewDetector.h"

HebrewDetector::HebrewDetector()
{
    inferenceEngine = std::make_unique<RealtimeInferenceEngine>();
    
    // Initialize with dummy model path for now
    inferenceEngine->initialize("het_detector.onnx");
    
    // Reserve audio buffer
    audioBuffer.reserve(PROCESSING_WINDOW_SIZE);
}

HebrewDetector::~HebrewDetector()
{
    inferenceEngine->shutdown();
}

void HebrewDetector::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    
    // Clear audio buffer
    audioBuffer.clear();
}

void HebrewDetector::reset()
{
    // Reset internal state
    audioBuffer.clear();
    lastConfidence.store(0.0f);
    smoothedConfidence.store(0.0f);
}

float HebrewDetector::processAudio(const juce::AudioBuffer<float>& buffer, float sensitivity)
{
    // Convert audio buffer to mono feature vector
    auto features = convertAudioToFeatures(buffer);
    
    // Accumulate audio samples for processing window
    audioBuffer.insert(audioBuffer.end(), features.begin(), features.end());
    
    // Process when we have enough samples
    if (audioBuffer.size() >= PROCESSING_WINDOW_SIZE)
    {
        // Submit inference request (non-blocking)
        std::vector<float> processingWindow(
            audioBuffer.begin(), 
            audioBuffer.begin() + PROCESSING_WINDOW_SIZE
        );
        
        inferenceEngine->submitInference(processingWindow);
        
        // Remove processed samples (with 50% overlap)
        size_t samplesToRemove = PROCESSING_WINDOW_SIZE / 2;
        audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + samplesToRemove);
    }
    
    // Check for latest inference results
    InferenceResult result;
    if (inferenceEngine->getLatestResult(result))
    {
        updateSmoothedConfidence(result.confidence);
    }
    
    // Apply post-processing with sensitivity
    float currentConfidence = smoothedConfidence.load();
    return applyPostProcessing(currentConfidence, sensitivity);
}

float HebrewDetector::getAverageLatency() const
{
    return inferenceEngine ? inferenceEngine->getAverageLatency() : 0.0f;
}

bool HebrewDetector::isHealthy() const
{
    return inferenceEngine ? inferenceEngine->isHealthy() : false;
}

std::vector<float> HebrewDetector::convertAudioToFeatures(const juce::AudioBuffer<float>& buffer)
{
    std::vector<float> features;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Convert to mono
    features.reserve(numSamples);
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float monoSample = 0.0f;
        
        // Average all channels to mono
        for (int channel = 0; channel < numChannels; ++channel)
        {
            monoSample += buffer.getSample(channel, sample);
        }
        
        if (numChannels > 0)
            monoSample /= numChannels;
            
        features.push_back(monoSample);
    }
    
    return features;
}

float HebrewDetector::applyPostProcessing(float rawConfidence, float sensitivity)
{
    // Apply sensitivity scaling
    // Sensitivity 0.0 = very strict (higher threshold)
    // Sensitivity 1.0 = very loose (lower threshold)
    
    float threshold = 0.3f * (1.0f - sensitivity); // Threshold decreases as sensitivity increases
    float adjustedConfidence = (rawConfidence - threshold) / (1.0f - threshold);
    
    // Clamp to [0, 1] range
    return std::max(0.0f, std::min(1.0f, adjustedConfidence));
}

void HebrewDetector::updateSmoothedConfidence(float newConfidence)
{
    // Exponential moving average for smooth confidence updates
    const float alpha = 0.2f; // Smoothing factor
    
    float currentSmoothed = smoothedConfidence.load();
    float newSmoothed = alpha * newConfidence + (1.0f - alpha) * currentSmoothed;
    
    smoothedConfidence.store(newSmoothed);
    lastConfidence.store(newConfidence);
}