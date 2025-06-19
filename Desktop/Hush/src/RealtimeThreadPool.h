#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <chrono>

// Platform-specific includes for thread priority
#ifdef _WIN32
    #include <windows.h>
    #include <processthreadsapi.h>
#elif defined(__APPLE__)
    #include <pthread.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#elif defined(__linux__)
    #include <pthread.h>
    #include <sched.h>
#endif

#include "RingBuffer.h"
#include "AiInference.h"

namespace KhDetector {

/**
 * @brief Real-time thread pool for audio processing tasks
 * 
 * This thread pool is designed for audio applications where tasks need to be
 * processed with lower priority than the audio thread but still with real-time
 * characteristics. It automatically sizes itself to CPU_CORES-1 to leave
 * resources for the audio thread.
 */
class RealtimeThreadPool
{
public:
    /**
     * @brief Task function type
     */
    using Task = std::function<void()>;

    /**
     * @brief Thread priority levels
     */
    enum class Priority
    {
        Low,        // Below normal priority
        Normal,     // Normal priority  
        High,       // Above normal priority
        Realtime    // Real-time priority (use with caution)
    };

    /**
     * @brief Construct thread pool with automatic sizing
     * 
     * @param priority Thread priority relative to audio thread
     * @param frameSize Expected frame size for processing (for optimization)
     */
    explicit RealtimeThreadPool(Priority priority = Priority::Low, int frameSize = 320);

    /**
     * @brief Construct thread pool with custom thread count
     * 
     * @param numThreads Number of worker threads
     * @param priority Thread priority relative to audio thread
     * @param frameSize Expected frame size for processing
     */
    RealtimeThreadPool(int numThreads, Priority priority = Priority::Low, int frameSize = 320);

    /**
     * @brief Destructor - stops all threads and waits for completion
     */
    ~RealtimeThreadPool();

    // Non-copyable and non-movable
    RealtimeThreadPool(const RealtimeThreadPool&) = delete;
    RealtimeThreadPool& operator=(const RealtimeThreadPool&) = delete;
    RealtimeThreadPool(RealtimeThreadPool&&) = delete;
    RealtimeThreadPool& operator=(RealtimeThreadPool&&) = delete;

    /**
     * @brief Start the thread pool
     * 
     * @param ringBuffer Ring buffer to process frames from
     * @param aiInference AI inference engine to use for processing
     * @param processingIntervalMs Interval between processing batches (default: 20ms)
     */
    void start(RingBuffer<float, 2048>* ringBuffer, 
               AiInference* aiInference,
               int processingIntervalMs = 20);

    /**
     * @brief Stop the thread pool gracefully
     */
    void stop();

    /**
     * @brief Check if the thread pool is running
     */
    bool isRunning() const { return mRunning.load(); }

    /**
     * @brief Get the number of worker threads
     */
    int getThreadCount() const { return static_cast<int>(mWorkerThreads.size()); }

    /**
     * @brief Get processing statistics
     */
    struct Statistics
    {
        std::atomic<uint64_t> framesProcessed{0};
        std::atomic<uint64_t> totalProcessingTimeUs{0};
        std::atomic<uint64_t> droppedFrames{0};
        std::atomic<double> averageProcessingTimeMs{0.0};
        std::atomic<double> cpuUsagePercent{0.0};
    };

    const Statistics& getStatistics() const { return mStats; }

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Submit a custom task to the thread pool
     * 
     * @param task Task to execute
     * @return true if task was queued, false if queue is full
     */
    bool submitTask(Task task);

    /**
     * @brief Get current task queue size
     */
    size_t getQueueSize() const;

private:
    // Configuration
    int mFrameSize;
    Priority mPriority;
    int mProcessingIntervalMs;
    
    // Threading
    std::vector<std::thread> mWorkerThreads;
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mShouldStop{false};
    
    // Task queue
    mutable std::mutex mTaskMutex;
    std::condition_variable mTaskCondition;
    std::queue<Task> mTaskQueue;
    static constexpr size_t kMaxQueueSize = 64;
    
    // Audio processing
    RingBuffer<float, 2048>* mRingBuffer = nullptr;
    AiInference* mAiInference = nullptr;
    
    // Statistics
    mutable Statistics mStats;
    std::chrono::steady_clock::time_point mLastStatsUpdate;
    
    // Processing buffers (per-thread)
    thread_local static std::vector<float> tProcessingFrame;
    
    /**
     * @brief Get optimal number of threads for audio processing
     */
    static int getOptimalThreadCount();
    
    /**
     * @brief Set thread priority for current thread
     */
    void setThreadPriority(Priority priority);
    
    /**
     * @brief Main worker thread function
     */
    void workerThreadMain();
    
    /**
     * @brief Main AI processing thread function
     */
    void aiProcessingThreadMain();
    
    /**
     * @brief Process a single audio frame with AI inference
     */
    void processAudioFrame(const float* frameData, int frameSize);
    
    /**
     * @brief Update performance statistics
     */
    void updateStatistics(std::chrono::microseconds processingTime);
    
    /**
     * @brief Check if enough time has passed for next processing cycle
     */
    bool shouldProcessNextFrame();
    
    // Timing
    std::chrono::steady_clock::time_point mLastProcessingTime;
};

/**
 * @brief RAII helper for thread priority management
 */
class ThreadPriorityGuard
{
public:
    explicit ThreadPriorityGuard(RealtimeThreadPool::Priority priority);
    ~ThreadPriorityGuard();
    
private:
    bool mPriorityChanged = false;
#ifdef _WIN32
    int mOriginalPriority;
#elif defined(__APPLE__) || defined(__linux__)
    int mOriginalPolicy;
    struct sched_param mOriginalParam;
#endif
};

} // namespace KhDetector 