#include "HebrewDetector.h"

HebrewDetector::HebrewDetector()
{
    modelInterface = std::make_unique<ONNXInterface>();
    
    // Load a dummy model path for now
    modelInterface->loadModel("dummy_model.onnx");
}

HebrewDetector::~HebrewDetector()
{
}

void HebrewDetector::prepare(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
}

void HebrewDetector::reset()
{
    // Reset any internal state
}

float HebrewDetector::processAudio(const juce::AudioBuffer<float>& buffer, float sensitivity)
{
    // Convert audio buffer to feature vector
    auto features = convertAudioToFeatures(buffer);
    
    // Run AI inference
    auto results = modelInterface->runInference(features);
    
    // Extract confidence (assuming single output)
    float rawConfidence = results.empty() ? 0.0f : results[0];
    
    // Apply post-processing with sensitivity
    return applyPostProcessing(rawConfidence, sensitivity);
}

std::vector<float> HebrewDetector::convertAudioToFeatures(const juce::AudioBuffer<float>& buffer)
{
    std::vector<float> features;
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Convert to mono if needed and create feature vector
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
    
    float threshold = 0.5f * (1.0f - sensitivity); // Threshold decreases as sensitivity increases
    float adjustedConfidence = (rawConfidence - threshold) / (1.0f - threshold);
    
    // Clamp to [0, 1] range
    return std::max(0.0f, std::min(1.0f, adjustedConfidence));
}