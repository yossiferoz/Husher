#include "AiInference.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace KhDetector {

AiInference::AiInference(const ModelConfig& config)
    : mConfig(config)
{
    // Initialize buffers
    mInputBuffer.reserve(config.inputSize);
    mOutputBuffer.reserve(config.outputSize);
    mNormalizedInput.reserve(config.inputSize);
    
    // Set default normalization if not provided
    if (mConfig.normalizationMean.empty()) {
        mConfig.normalizationMean.resize(1, 0.0f);
    }
    if (mConfig.normalizationStd.empty()) {
        mConfig.normalizationStd.resize(1, 1.0f);
    }
    
    detectHardwareCapabilities();
    
    // Initialize post-processor with default configuration
    mPostProcessor = createPostProcessor(0.6f, 5);
}

AiInference::~AiInference() = default;

bool AiInference::initialize(const ModelConfig& config)
{
    if (mInitialized.load()) {
        std::cout << "AiInference: Already initialized" << std::endl;
        return true;
    }
    
    mConfig = config;
    
    // Resize buffers
    mInputBuffer.resize(config.inputSize);
    mOutputBuffer.resize(config.outputSize);
    mNormalizedInput.resize(config.inputSize);
    
    // Set default normalization if not provided
    if (mConfig.normalizationMean.empty()) {
        mConfig.normalizationMean.resize(1, 0.0f);
    }
    if (mConfig.normalizationStd.empty()) {
        mConfig.normalizationStd.resize(1, 1.0f);
    }
    
    detectHardwareCapabilities();
    
    // Initialize post-processor if not already created
    if (!mPostProcessor) {
        mPostProcessor = createPostProcessor(config.confidenceThreshold, 5);
    }
    
    // Simulate initialization time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    mInitialized.store(true);
    
    std::cout << "AiInference: Initialized with input size " << config.inputSize 
              << ", output size " << config.outputSize 
              << ", sample rate " << config.sampleRate << " Hz" << std::endl;
    
    return true;
}

bool AiInference::loadModel(const std::string& modelPath)
{
    mConfig.modelPath = modelPath;
    
    // Simulate model loading time
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::cout << "AiInference: Loaded model from " << modelPath << std::endl;
    return true;
}

AiInference::InferenceResult AiInference::run(const float* audioData, int numSamples)
{
    InferenceResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!mInitialized.load() || !audioData || numSamples <= 0) {
        result.success = false;
        return result;
    }
    
    if (numSamples != mConfig.inputSize) {
        std::cerr << "AiInference: Input size mismatch. Expected " << mConfig.inputSize 
                  << ", got " << numSamples << std::endl;
        result.success = false;
        return result;
    }
    
    try {
        // Normalize input
        normalizeInput(audioData, numSamples, mNormalizedInput.data());
        
        // Run inference
        bool inferenceSuccess = runInferenceInternal(
            mNormalizedInput.data(), 
            numSamples, 
            mOutputBuffer.data(), 
            mConfig.outputSize
        );
        
        if (inferenceSuccess) {
            // Post-process output
            postprocessOutput(mOutputBuffer.data(), mConfig.outputSize, result);
            result.success = true;
        } else {
            result.success = false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "AiInference: Exception during inference: " << e.what() << std::endl;
        result.success = false;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    updateStatistics(result);
    
    // Call callback if set
    if (mCallback && result.success) {
        try {
            mCallback(result);
        } catch (const std::exception& e) {
            std::cerr << "AiInference: Callback exception: " << e.what() << std::endl;
        }
    }
    
    return result;
}

AiInference::InferenceResult AiInference::run(const std::vector<float>& audioFrame)
{
    return run(audioFrame.data(), static_cast<int>(audioFrame.size()));
}

void AiInference::resetStatistics()
{
    mStats.totalInferences.store(0);
    mStats.successfulInferences.store(0);
    mStats.failedInferences.store(0);
    mStats.totalProcessingTimeUs.store(0);
    mStats.averageProcessingTimeMs.store(0.0);
    mStats.averageConfidence.store(0.0);
}

void AiInference::setInferenceCallback(InferenceCallback callback)
{
    mCallback = std::move(callback);
}

void AiInference::warmup(int numWarmupRuns)
{
    if (!mInitialized.load()) {
        std::cout << "AiInference: Cannot warmup - not initialized" << std::endl;
        return;
    }
    
    std::cout << "AiInference: Warming up with " << numWarmupRuns << " runs..." << std::endl;
    
    // Create dummy audio data
    std::vector<float> dummyAudio(mConfig.inputSize, 0.0f);
    
    // Generate some realistic audio-like data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.1f);
    
    for (int i = 0; i < mConfig.inputSize; ++i) {
        dummyAudio[i] = dist(gen);
    }
    
    // Run warmup inferences
    for (int i = 0; i < numWarmupRuns; ++i) {
        auto result = run(dummyAudio.data(), mConfig.inputSize);
        if (!result.success) {
            std::cerr << "AiInference: Warmup run " << i << " failed" << std::endl;
        }
    }
    
    std::cout << "AiInference: Warmup completed" << std::endl;
}

void AiInference::normalizeInput(const float* input, int numSamples, float* output)
{
    // Simple normalization using configured mean and std
    float mean = mConfig.normalizationMean.empty() ? 0.0f : mConfig.normalizationMean[0];
    float std = mConfig.normalizationStd.empty() ? 1.0f : mConfig.normalizationStd[0];
    
    for (int i = 0; i < numSamples; ++i) {
        output[i] = (input[i] - mean) / std;
    }
}

void AiInference::postprocessOutput(const float* output, int numOutputs, InferenceResult& result)
{
    result.predictions.resize(numOutputs);
    std::copy(output, output + numOutputs, result.predictions.begin());
    
    // Calculate raw confidence as the maximum output value (assuming softmax-like output)
    float rawConfidence = 0.0f;
    if (numOutputs > 0) {
        rawConfidence = *std::max_element(output, output + numOutputs);
        rawConfidence = std::clamp(rawConfidence, 0.0f, 1.0f);
    }
    
    // Apply post-processing (median filtering + threshold detection)
    if (mPostProcessor) {
        result.confidence = mPostProcessor->processConfidence(rawConfidence);
        
        // Generate classification based on hit detection
        if (mPostProcessor->hasHit()) {
            result.classification = "detected";
        } else {
            result.classification = "not_detected";
        }
    } else {
        // Fallback to raw confidence if no post-processor
        result.confidence = rawConfidence;
        if (result.confidence > mConfig.confidenceThreshold) {
            result.classification = "detected";
        } else {
            result.classification = "not_detected";
        }
    }
}

void AiInference::updateStatistics(const InferenceResult& result)
{
    mStats.totalInferences.fetch_add(1);
    
    if (result.success) {
        mStats.successfulInferences.fetch_add(1);
        
        // Update average confidence
        auto totalSuccessful = mStats.successfulInferences.load();
        if (totalSuccessful > 0) {
            double currentAvg = mStats.averageConfidence.load();
            double newAvg = currentAvg + (result.confidence - currentAvg) / totalSuccessful;
            mStats.averageConfidence.store(newAvg);
        }
    } else {
        mStats.failedInferences.fetch_add(1);
    }
    
    // Update processing time statistics
    auto totalTime = mStats.totalProcessingTimeUs.fetch_add(result.processingTime.count()) 
                     + result.processingTime.count();
    auto totalInferences = mStats.totalInferences.load();
    
    if (totalInferences > 0) {
        double avgTimeMs = static_cast<double>(totalTime) / (totalInferences * 1000.0);
        mStats.averageProcessingTimeMs.store(avgTimeMs);
    }
}

bool AiInference::runInferenceInternal(const float* input, int inputSize, float* output, int outputSize)
{
    // This is a stub implementation that generates realistic dummy results
    // In a real implementation, this would call actual ML framework APIs
    
    // Simulate some processing time
    if (mConfig.useGpu && mGpuAvailable) {
        // GPU inference is typically faster but has some overhead
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    } else {
        // CPU inference
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    
    // Generate realistic output based on input characteristics
    float inputEnergy = 0.0f;
    for (int i = 0; i < inputSize; ++i) {
        inputEnergy += input[i] * input[i];
    }
    inputEnergy = std::sqrt(inputEnergy / inputSize);
    
    // Generate output based on input energy and some random factors
    float baseResult = generateTestResult(input, inputSize);
    
    for (int i = 0; i < outputSize; ++i) {
        // Each output represents a different class probability
        float probability = baseResult;
        
        if (i > 0) {
            // Other classes get lower probabilities
            probability *= (1.0f - baseResult) / (outputSize - 1);
        }
        
        output[i] = std::clamp(probability, 0.0f, 1.0f);
    }
    
    return true;
}

void AiInference::detectHardwareCapabilities()
{
    // Stub implementation - in reality would check for CUDA, OpenCL, etc.
    mGpuAvailable.store(false);
    
    #ifdef __APPLE__
        // Check for Metal Performance Shaders availability
        // This is a simplified check
        mGpuAvailable.store(true);
    #elif defined(_WIN32)
        // Check for DirectML or CUDA
        // This is a simplified check
        mGpuAvailable.store(false);
    #elif defined(__linux__)
        // Check for CUDA or OpenCL
        // This is a simplified check
        mGpuAvailable.store(false);
    #endif
    
    std::cout << "AiInference: GPU acceleration " 
              << (mGpuAvailable.load() ? "available" : "not available") << std::endl;
}

float AiInference::generateTestResult(const float* audioData, int numSamples)
{
    // Generate a realistic test result based on audio characteristics
    
    // Calculate some basic audio features
    float rms = 0.0f;
    float zcr = 0.0f; // Zero crossing rate
    float spectralCentroid = 0.0f;
    
    // RMS (energy)
    for (int i = 0; i < numSamples; ++i) {
        rms += audioData[i] * audioData[i];
    }
    rms = std::sqrt(rms / numSamples);
    
    // Zero crossing rate
    for (int i = 1; i < numSamples; ++i) {
        if ((audioData[i] >= 0.0f) != (audioData[i-1] >= 0.0f)) {
            zcr += 1.0f;
        }
    }
    zcr /= (numSamples - 1);
    
    // Simple spectral centroid approximation
    float sumMagnitude = 0.0f;
    float weightedSum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float magnitude = std::abs(audioData[i]);
        sumMagnitude += magnitude;
        weightedSum += magnitude * i;
    }
    if (sumMagnitude > 0.0f) {
        spectralCentroid = weightedSum / sumMagnitude;
        spectralCentroid /= numSamples; // Normalize
    }
    
    // Combine features to generate a detection probability
    float result = 0.0f;
    
    // Energy-based component (voices typically have moderate energy)
    if (rms > 0.01f && rms < 0.3f) {
        result += 0.3f;
    }
    
    // Zero crossing rate component (human speech typically has moderate ZCR)
    if (zcr > 0.02f && zcr < 0.2f) {
        result += 0.3f;
    }
    
    // Spectral centroid component
    if (spectralCentroid > 0.2f && spectralCentroid < 0.8f) {
        result += 0.2f;
    }
    
    // Add some random variation to simulate model uncertainty
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
    result += noise(gen);
    
    return std::clamp(result, 0.0f, 1.0f);
}

// Factory functions
std::unique_ptr<AiInference> createAiInference(const AiInference::ModelConfig& config)
{
    auto inference = std::make_unique<AiInference>(config);
    if (inference->initialize(config)) {
        return inference;
    }
    return nullptr;
}

AiInference::ModelConfig createDefaultModelConfig()
{
    AiInference::ModelConfig config;
    config.inputSize = 320;           // 20ms at 16kHz
    config.outputSize = 1;            // Binary detection
    config.sampleRate = 16000.0f;     // 16kHz
    config.useGpu = false;            // Start with CPU
    config.numThreads = 1;            // Single threaded inference
    config.confidenceThreshold = 0.5f;
    
    // Default normalization (no normalization)
    config.normalizationMean = {0.0f};
    config.normalizationStd = {1.0f};
    
    return config;
}

} // namespace KhDetector 