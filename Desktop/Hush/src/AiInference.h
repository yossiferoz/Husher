#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>

#include "PostProcessor.h"

namespace KhDetector {

/**
 * @brief Stub AI inference engine for audio processing
 * 
 * This is a placeholder implementation that can be replaced with actual
 * AI inference frameworks like TensorFlow Lite, PyTorch Mobile, ONNX Runtime, etc.
 */
class AiInference
{
public:
    /**
     * @brief Inference result structure
     */
    struct InferenceResult
    {
        std::vector<float> predictions;     // Model predictions/outputs
        float confidence = 0.0f;           // Confidence score (0.0 - 1.0)
        std::string classification;        // Classification result if applicable
        std::chrono::microseconds processingTime{0}; // Time taken for inference
        bool success = false;              // Whether inference succeeded
    };

    /**
     * @brief Model configuration
     */
    struct ModelConfig
    {
        std::string modelPath;             // Path to model file
        int inputSize = 320;               // Expected input frame size
        int outputSize = 1;                // Number of output predictions
        float sampleRate = 16000.0f;       // Expected sample rate
        bool useGpu = false;               // Whether to use GPU acceleration
        int numThreads = 1;                // Number of inference threads
        
        // Model-specific parameters
        std::vector<float> normalizationMean;
        std::vector<float> normalizationStd;
        float confidenceThreshold = 0.5f;
    };

    /**
     * @brief Constructor
     */
    explicit AiInference(const ModelConfig& config = ModelConfig{});

    /**
     * @brief Destructor
     */
    ~AiInference();

    // Non-copyable, but movable
    AiInference(const AiInference&) = delete;
    AiInference& operator=(const AiInference&) = delete;
    AiInference(AiInference&&) = default;
    AiInference& operator=(AiInference&&) = default;

    /**
     * @brief Initialize the inference engine
     * 
     * @param config Model configuration
     * @return true if initialization succeeded
     */
    bool initialize(const ModelConfig& config);

    /**
     * @brief Load model from file
     * 
     * @param modelPath Path to model file
     * @return true if model loaded successfully
     */
    bool loadModel(const std::string& modelPath);

    /**
     * @brief Run inference on audio frame
     * 
     * @param audioData Pointer to audio samples
     * @param numSamples Number of samples in the frame
     * @return Inference result
     */
    InferenceResult run(const float* audioData, int numSamples);

    /**
     * @brief Run inference on audio frame (vector version)
     * 
     * @param audioFrame Audio samples as vector
     * @return Inference result
     */
    InferenceResult run(const std::vector<float>& audioFrame);

    /**
     * @brief Check if the inference engine is ready
     */
    bool isReady() const { return mInitialized.load(); }

    /**
     * @brief Get model configuration
     */
    const ModelConfig& getConfig() const { return mConfig; }

    /**
     * @brief Get inference statistics
     */
    struct Statistics
    {
        std::atomic<uint64_t> totalInferences{0};
        std::atomic<uint64_t> successfulInferences{0};
        std::atomic<uint64_t> failedInferences{0};
        std::atomic<uint64_t> totalProcessingTimeUs{0};
        std::atomic<double> averageProcessingTimeMs{0.0};
        std::atomic<double> averageConfidence{0.0};
    };

    const Statistics& getStatistics() const { return mStats; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    /**
     * @brief Set inference callback for real-time results
     * 
     * @param callback Function to call with inference results
     */
    using InferenceCallback = std::function<void(const InferenceResult&)>;
    void setInferenceCallback(InferenceCallback callback);

    /**
     * @brief Warmup the inference engine with dummy data
     * 
     * This can help ensure consistent performance by pre-loading
     * any lazy-initialized components.
     */
    void warmup(int numWarmupRuns = 5);

    /**
     * @brief Get recommended frame size for optimal performance
     */
    int getRecommendedFrameSize() const { return mConfig.inputSize; }

    /**
     * @brief Check if GPU acceleration is available and enabled
     */
    bool isGpuAccelerated() const { return mConfig.useGpu && mGpuAvailable; }

    /**
     * @brief Get post-processor for hit detection
     */
    PostProcessor* getPostProcessor() { return mPostProcessor.get(); }
    const PostProcessor* getPostProcessor() const { return mPostProcessor.get(); }

    /**
     * @brief Check if there's currently a hit detected (thread-safe)
     */
    bool hasHit() const { return mPostProcessor ? mPostProcessor->hasHit() : false; }

private:
    ModelConfig mConfig;
    std::atomic<bool> mInitialized{false};
    std::atomic<bool> mGpuAvailable{false};
    
    // Statistics
    mutable Statistics mStats;
    
    // Callback
    InferenceCallback mCallback;
    
    // Post-processing
    std::unique_ptr<PostProcessor> mPostProcessor;
    
    // Internal processing buffers
    std::vector<float> mInputBuffer;
    std::vector<float> mOutputBuffer;
    std::vector<float> mNormalizedInput;
    
    // Timing
    std::chrono::steady_clock::time_point mLastInferenceTime;
    
    /**
     * @brief Normalize input audio data
     */
    void normalizeInput(const float* input, int numSamples, float* output);
    
    /**
     * @brief Apply post-processing to model outputs
     */
    void postprocessOutput(const float* output, int numOutputs, InferenceResult& result);
    
    /**
     * @brief Update inference statistics
     */
    void updateStatistics(const InferenceResult& result);
    
    /**
     * @brief Stub inference implementation
     * 
     * This is where actual ML framework calls would go.
     * Currently generates realistic dummy results for testing.
     */
    bool runInferenceInternal(const float* input, int inputSize, float* output, int outputSize);
    
    /**
     * @brief Check hardware capabilities
     */
    void detectHardwareCapabilities();
    
    /**
     * @brief Generate realistic test signal for demonstration
     */
    float generateTestResult(const float* audioData, int numSamples);
};

/**
 * @brief Factory function for creating AI inference engines
 * 
 * This allows for easy swapping of different inference backends
 * without changing the main application code.
 */
std::unique_ptr<AiInference> createAiInference(const AiInference::ModelConfig& config);

/**
 * @brief Helper function to create default model configuration
 */
AiInference::ModelConfig createDefaultModelConfig();

} // namespace KhDetector 