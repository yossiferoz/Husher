#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "ONNXInterface.h"

struct InferenceRequest
{
    std::vector<float> audioData;
    std::chrono::high_resolution_clock::time_point timestamp;
    int requestId;
};

struct InferenceResult
{
    float confidence;
    std::chrono::high_resolution_clock::time_point timestamp;
    int requestId;
    std::chrono::milliseconds processingTime;
};

class RealtimeInferenceEngine
{
public:
    RealtimeInferenceEngine();
    ~RealtimeInferenceEngine();
    
    void initialize(const std::string& modelPath);
    void shutdown();
    
    // Submit audio for inference (non-blocking)
    int submitInference(const std::vector<float>& audioData);
    
    // Get latest result (non-blocking)
    bool getLatestResult(InferenceResult& result);
    
    // Performance monitoring
    float getAverageLatency() const { return averageLatency.load(); }
    int getQueueSize() const;
    bool isHealthy() const { return isRunning.load() && !errorState.load(); }
    
private:
    // Threading
    std::thread inferenceThread;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> errorState{false};
    
    // Request/result queues
    std::queue<InferenceRequest> requestQueue;
    std::queue<InferenceResult> resultQueue;
    std::mutex requestMutex;
    std::mutex resultMutex;
    std::condition_variable requestCondition;
    
    // AI model interface
    std::unique_ptr<ONNXInterface> modelInterface;
    
    // Performance tracking
    std::atomic<float> averageLatency{0.0f};
    std::atomic<int> nextRequestId{0};
    static constexpr size_t MAX_QUEUE_SIZE = 16;
    static constexpr size_t MAX_RESULT_QUEUE_SIZE = 8;
    
    // Thread worker function
    void inferenceWorker();
    
    // Utility functions
    void updateLatencyMetrics(std::chrono::milliseconds processingTime);
    void cleanupOldResults();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RealtimeInferenceEngine)
};