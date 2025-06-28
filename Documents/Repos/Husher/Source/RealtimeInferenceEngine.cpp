#include "RealtimeInferenceEngine.h"
#include <algorithm>
#include <chrono>

RealtimeInferenceEngine::RealtimeInferenceEngine()
{
    modelInterface = std::make_unique<ONNXInterface>();
}

RealtimeInferenceEngine::~RealtimeInferenceEngine()
{
    shutdown();
}

void RealtimeInferenceEngine::initialize(const std::string& modelPath)
{
    if (isRunning.load())
    {
        shutdown();
    }
    
    // Load the model
    bool modelLoaded = modelInterface->loadModel(modelPath);
    if (!modelLoaded)
    {
        errorState.store(true);
        return;
    }
    
    // Reset state
    errorState.store(false);
    nextRequestId.store(0);
    averageLatency.store(0.0f);
    
    // Clear queues
    {
        std::lock_guard<std::mutex> requestLock(requestMutex);
        std::queue<InferenceRequest> emptyRequestQueue;
        requestQueue.swap(emptyRequestQueue);
    }
    
    {
        std::lock_guard<std::mutex> resultLock(resultMutex);
        std::queue<InferenceResult> emptyResultQueue;
        resultQueue.swap(emptyResultQueue);
    }
    
    // Start inference thread
    isRunning.store(true);
    inferenceThread = std::thread(&RealtimeInferenceEngine::inferenceWorker, this);
}

void RealtimeInferenceEngine::shutdown()
{
    if (isRunning.load())
    {
        isRunning.store(false);
        requestCondition.notify_all();
        
        if (inferenceThread.joinable())
        {
            inferenceThread.join();
        }
    }
}

int RealtimeInferenceEngine::submitInference(const std::vector<float>& audioData)
{
    if (!isRunning.load() || errorState.load())
    {
        return -1;
    }
    
    // Create inference request
    InferenceRequest request;
    request.audioData = audioData;
    request.timestamp = std::chrono::high_resolution_clock::now();
    request.requestId = nextRequestId.fetch_add(1);
    
    // Add to queue (drop oldest if queue is full)
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        
        if (requestQueue.size() >= MAX_QUEUE_SIZE)
        {
            // Drop oldest request to maintain real-time performance
            requestQueue.pop();
        }
        
        requestQueue.push(std::move(request));
    }
    
    // Notify inference thread
    requestCondition.notify_one();
    
    return request.requestId;
}

bool RealtimeInferenceEngine::getLatestResult(InferenceResult& result)
{
    std::lock_guard<std::mutex> lock(resultMutex);
    
    if (resultQueue.empty())
    {
        return false;
    }
    
    // Get the latest result (most recent)
    result = resultQueue.back();
    
    // Clean up old results
    cleanupOldResults();
    
    return true;
}

int RealtimeInferenceEngine::getQueueSize() const
{
    // Note: We need to cast away const for mutex operations
    // This is safe because getQueueSize() is logically const
    auto& non_const_mutex = const_cast<std::mutex&>(requestMutex);
    std::lock_guard<std::mutex> lock(non_const_mutex);
    return static_cast<int>(requestQueue.size());
}

void RealtimeInferenceEngine::inferenceWorker()
{
    while (isRunning.load())
    {
        InferenceRequest request;
        bool hasRequest = false;
        
        // Wait for request
        {
            std::unique_lock<std::mutex> lock(requestMutex);
            requestCondition.wait(lock, [this] { 
                return !requestQueue.empty() || !isRunning.load(); 
            });
            
            if (!isRunning.load())
            {
                break;
            }
            
            if (!requestQueue.empty())
            {
                request = std::move(requestQueue.front());
                requestQueue.pop();
                hasRequest = true;
            }
        }
        
        if (!hasRequest)
        {
            continue;
        }
        
        // Process inference
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try
        {
            // Run AI inference
            auto inferenceResults = modelInterface->runInference(request.audioData);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            // Extract confidence (assuming single output)
            float confidence = inferenceResults.empty() ? 0.0f : inferenceResults[0];
            
            // Create result
            InferenceResult result;
            result.confidence = confidence;
            result.timestamp = request.timestamp;
            result.requestId = request.requestId;
            result.processingTime = processingTime;
            
            // Add to result queue
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                
                if (resultQueue.size() >= MAX_RESULT_QUEUE_SIZE)
                {
                    resultQueue.pop(); // Remove oldest result
                }
                
                resultQueue.push(std::move(result));
            }
            
            // Update performance metrics
            updateLatencyMetrics(processingTime);
        }
        catch (const std::exception& e)
        {
            // Log error and continue
            errorState.store(true);
        }
    }
}

void RealtimeInferenceEngine::updateLatencyMetrics(std::chrono::milliseconds processingTime)
{
    // Simple exponential moving average
    float currentLatency = static_cast<float>(processingTime.count());
    float currentAverage = averageLatency.load();
    
    const float alpha = 0.1f; // Smoothing factor
    float newAverage = alpha * currentLatency + (1.0f - alpha) * currentAverage;
    
    averageLatency.store(newAverage);
}

void RealtimeInferenceEngine::cleanupOldResults()
{
    // Remove all but the most recent result to save memory
    while (resultQueue.size() > 1)
    {
        resultQueue.pop();
    }
}