#include "RealtimeThreadPool.h"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace KhDetector {

// Thread-local storage for processing buffers
thread_local std::vector<float> RealtimeThreadPool::tProcessingFrame;

RealtimeThreadPool::RealtimeThreadPool(Priority priority, int frameSize)
    : RealtimeThreadPool(getOptimalThreadCount(), priority, frameSize)
{
}

RealtimeThreadPool::RealtimeThreadPool(int numThreads, Priority priority, int frameSize)
    : mFrameSize(frameSize)
    , mPriority(priority)
    , mProcessingIntervalMs(20)
    , mLastStatsUpdate(std::chrono::steady_clock::now())
    , mLastProcessingTime(std::chrono::steady_clock::now())
{
    // Ensure we have at least 1 thread but not more than hardware threads - 1
    int maxThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    numThreads = std::clamp(numThreads, 1, maxThreads);
    
    mWorkerThreads.reserve(numThreads);
    
    std::cout << "RealtimeThreadPool: Creating " << numThreads << " worker threads with frame size " 
              << frameSize << std::endl;
}

RealtimeThreadPool::~RealtimeThreadPool()
{
    stop();
}

void RealtimeThreadPool::start(RingBuffer<float, 2048>* ringBuffer, 
                               AiInference* aiInference,
                               int processingIntervalMs)
{
    if (mRunning.load()) {
        std::cout << "RealtimeThreadPool: Already running" << std::endl;
        return;
    }
    
    if (!ringBuffer || !aiInference) {
        std::cout << "RealtimeThreadPool: Invalid ring buffer or AI inference engine" << std::endl;
        return;
    }
    
    mRingBuffer = ringBuffer;
    mAiInference = aiInference;
    mProcessingIntervalMs = processingIntervalMs;
    mRunning.store(true);
    mShouldStop.store(false);
    
    // Create worker threads
    for (size_t i = 0; i < mWorkerThreads.capacity(); ++i) {
        if (i == 0) {
            // First thread is dedicated to AI processing
            mWorkerThreads.emplace_back(&RealtimeThreadPool::aiProcessingThreadMain, this);
        } else {
            // Other threads are general purpose workers
            mWorkerThreads.emplace_back(&RealtimeThreadPool::workerThreadMain, this);
        }
    }
    
    std::cout << "RealtimeThreadPool: Started with " << mWorkerThreads.size() 
              << " threads, processing interval: " << processingIntervalMs << "ms" << std::endl;
}

void RealtimeThreadPool::stop()
{
    if (!mRunning.load()) {
        return;
    }
    
    std::cout << "RealtimeThreadPool: Stopping..." << std::endl;
    
    mShouldStop.store(true);
    mRunning.store(false);
    
    // Wake up all waiting threads
    mTaskCondition.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : mWorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    mWorkerThreads.clear();
    
    // Clear any remaining tasks
    {
        std::lock_guard<std::mutex> lock(mTaskMutex);
        while (!mTaskQueue.empty()) {
            mTaskQueue.pop();
        }
    }
    
    std::cout << "RealtimeThreadPool: Stopped" << std::endl;
}

bool RealtimeThreadPool::submitTask(Task task)
{
    if (!mRunning.load() || !task) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(mTaskMutex);
        if (mTaskQueue.size() >= kMaxQueueSize) {
            return false; // Queue is full
        }
        mTaskQueue.push(std::move(task));
    }
    
    mTaskCondition.notify_one();
    return true;
}

size_t RealtimeThreadPool::getQueueSize() const
{
    std::lock_guard<std::mutex> lock(mTaskMutex);
    return mTaskQueue.size();
}

void RealtimeThreadPool::resetStatistics()
{
    mStats.framesProcessed.store(0);
    mStats.totalProcessingTimeUs.store(0);
    mStats.droppedFrames.store(0);
    mStats.averageProcessingTimeMs.store(0.0);
    mStats.cpuUsagePercent.store(0.0);
    mLastStatsUpdate = std::chrono::steady_clock::now();
}

int RealtimeThreadPool::getOptimalThreadCount()
{
    // Use CPU cores - 1 to leave room for the audio thread
    int hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads <= 1) {
        return 1;
    }
    
    // For audio processing, we typically don't need more than 4-6 threads
    return std::min(hardwareThreads - 1, 6);
}

void RealtimeThreadPool::setThreadPriority(Priority priority)
{
    try {
#ifdef _WIN32
        HANDLE currentThread = GetCurrentThread();
        int winPriority;
        
        switch (priority) {
            case Priority::Low:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case Priority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case Priority::High:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case Priority::Realtime:
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
        }
        
        SetThreadPriority(currentThread, winPriority);
        
#elif defined(__APPLE__)
        // macOS thread priority
        int policy = SCHED_OTHER;
        struct sched_param param;
        param.sched_priority = 0;
        
        switch (priority) {
            case Priority::Low:
                // Use default scheduling with lower priority
                break;
            case Priority::Normal:
                // Default priority
                param.sched_priority = 0;
                break;
            case Priority::High:
                param.sched_priority = 10;
                break;
            case Priority::Realtime:
                policy = SCHED_FIFO;
                param.sched_priority = 20;
                break;
        }
        
        pthread_setschedparam(pthread_self(), policy, &param);
        
#elif defined(__linux__)
        // Linux thread priority
        int policy = SCHED_OTHER;
        struct sched_param param;
        param.sched_priority = 0;
        
        switch (priority) {
            case Priority::Low:
                // Use nice value instead
                nice(5);
                break;
            case Priority::Normal:
                // Default priority
                break;
            case Priority::High:
                nice(-5);
                break;
            case Priority::Realtime:
                policy = SCHED_FIFO;
                param.sched_priority = 50;
                pthread_setschedparam(pthread_self(), policy, &param);
                break;
        }
#endif
    } catch (const std::exception& e) {
        std::cerr << "RealtimeThreadPool: Failed to set thread priority: " << e.what() << std::endl;
    }
}

void RealtimeThreadPool::workerThreadMain()
{
    // Set thread priority
    setThreadPriority(mPriority);
    
    // Initialize thread-local processing buffer
    if (tProcessingFrame.size() != static_cast<size_t>(mFrameSize)) {
        tProcessingFrame.resize(mFrameSize);
    }
    
    std::cout << "RealtimeThreadPool: Worker thread started" << std::endl;
    
    while (!mShouldStop.load()) {
        Task task;
        
        // Wait for a task
        {
            std::unique_lock<std::mutex> lock(mTaskMutex);
            mTaskCondition.wait(lock, [this] { 
                return mShouldStop.load() || !mTaskQueue.empty(); 
            });
            
            if (mShouldStop.load()) {
                break;
            }
            
            if (!mTaskQueue.empty()) {
                task = std::move(mTaskQueue.front());
                mTaskQueue.pop();
            }
        }
        
        // Execute the task
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "RealtimeThreadPool: Task execution failed: " << e.what() << std::endl;
            }
        }
    }
    
    std::cout << "RealtimeThreadPool: Worker thread finished" << std::endl;
}

void RealtimeThreadPool::aiProcessingThreadMain()
{
    // Set thread priority
    setThreadPriority(mPriority);
    
    // Initialize thread-local processing buffer
    if (tProcessingFrame.size() != static_cast<size_t>(mFrameSize)) {
        tProcessingFrame.resize(mFrameSize);
    }
    
    std::cout << "RealtimeThreadPool: AI processing thread started" << std::endl;
    
    while (!mShouldStop.load()) {
        if (!shouldProcessNextFrame()) {
            // Wait a bit before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        
        // Try to get a frame from the ring buffer
        if (mRingBuffer && mRingBuffer->size() >= static_cast<size_t>(mFrameSize)) {
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Pop frame from ring buffer
            size_t samplesPopped = mRingBuffer->pop_bulk(tProcessingFrame.data(), mFrameSize);
            
            if (samplesPopped == static_cast<size_t>(mFrameSize)) {
                // Process the frame
                processAudioFrame(tProcessingFrame.data(), mFrameSize);
                
                auto endTime = std::chrono::high_resolution_clock::now();
                auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                
                updateStatistics(processingTime);
                mLastProcessingTime = endTime;
            } else {
                // Not enough samples available
                mStats.droppedFrames.fetch_add(1);
            }
        } else {
            // Ring buffer is empty or doesn't have enough samples
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    std::cout << "RealtimeThreadPool: AI processing thread finished" << std::endl;
}

void RealtimeThreadPool::processAudioFrame(const float* frameData, int frameSize)
{
    if (!mAiInference || !frameData || frameSize <= 0) {
        return;
    }
    
    try {
        // Run AI inference on the audio frame
        auto result = mAiInference->run(frameData, frameSize);
        
        if (result.success) {
            mStats.framesProcessed.fetch_add(1);
            
            // For demonstration, we could log significant results
            if (result.confidence > 0.8f) {
                // High confidence detection - could trigger some action
            }
        } else {
            mStats.droppedFrames.fetch_add(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "RealtimeThreadPool: AI inference failed: " << e.what() << std::endl;
        mStats.droppedFrames.fetch_add(1);
    }
}

void RealtimeThreadPool::updateStatistics(std::chrono::microseconds processingTime)
{
    auto totalTime = mStats.totalProcessingTimeUs.fetch_add(processingTime.count()) + processingTime.count();
    auto totalFrames = mStats.framesProcessed.load();
    
    if (totalFrames > 0) {
        double avgTimeMs = static_cast<double>(totalTime) / (totalFrames * 1000.0);
        mStats.averageProcessingTimeMs.store(avgTimeMs);
        
        // Calculate CPU usage percentage
        double expectedTimeMs = static_cast<double>(mProcessingIntervalMs);
        double cpuUsage = (avgTimeMs / expectedTimeMs) * 100.0;
        mStats.cpuUsagePercent.store(std::min(cpuUsage, 100.0));
    }
}

bool RealtimeThreadPool::shouldProcessNextFrame()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastProcessingTime);
    return elapsed.count() >= mProcessingIntervalMs;
}

// ThreadPriorityGuard implementation
ThreadPriorityGuard::ThreadPriorityGuard(RealtimeThreadPool::Priority priority)
{
    try {
#ifdef _WIN32
        HANDLE currentThread = GetCurrentThread();
        mOriginalPriority = GetThreadPriority(currentThread);
        
        int winPriority;
        switch (priority) {
            case RealtimeThreadPool::Priority::Low:
                winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case RealtimeThreadPool::Priority::Normal:
                winPriority = THREAD_PRIORITY_NORMAL;
                break;
            case RealtimeThreadPool::Priority::High:
                winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case RealtimeThreadPool::Priority::Realtime:
                winPriority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
        }
        
        mPriorityChanged = SetThreadPriority(currentThread, winPriority) != 0;
        
#elif defined(__APPLE__) || defined(__linux__)
        pthread_getschedparam(pthread_self(), &mOriginalPolicy, &mOriginalParam);
        
        int newPolicy = SCHED_OTHER;
        struct sched_param newParam;
        newParam.sched_priority = 0;
        
        switch (priority) {
            case RealtimeThreadPool::Priority::Low:
                // Keep default
                break;
            case RealtimeThreadPool::Priority::Normal:
                newParam.sched_priority = 0;
                break;
            case RealtimeThreadPool::Priority::High:
                newParam.sched_priority = 10;
                break;
            case RealtimeThreadPool::Priority::Realtime:
                newPolicy = SCHED_FIFO;
                newParam.sched_priority = 20;
                break;
        }
        
        mPriorityChanged = pthread_setschedparam(pthread_self(), newPolicy, &newParam) == 0;
#endif
    } catch (...) {
        mPriorityChanged = false;
    }
}

ThreadPriorityGuard::~ThreadPriorityGuard()
{
    if (!mPriorityChanged) {
        return;
    }
    
    try {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), mOriginalPriority);
#elif defined(__APPLE__) || defined(__linux__)
        pthread_setschedparam(pthread_self(), mOriginalPolicy, &mOriginalParam);
#endif
    } catch (...) {
        // Ignore errors during cleanup
    }
}

} // namespace KhDetector 