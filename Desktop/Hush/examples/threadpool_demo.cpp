#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <atomic>
#include <iomanip>

#include "RealtimeThreadPool.h"
#include "AiInference.h"
#include "RingBuffer.h"

using namespace KhDetector;

/**
 * @brief Demonstration of RealtimeThreadPool with AI inference processing
 * 
 * This example shows how to use the thread pool for real-time audio processing
 * with AI inference running at lower priority than the audio thread.
 */
class AudioThreadPoolDemo
{
public:
    AudioThreadPoolDemo()
        : mRingBuffer(std::make_unique<RingBuffer<float, 2048>>())
        , mStopFlag(false)
    {
        // Initialize AI inference
        auto config = createDefaultModelConfig();
        config.inputSize = kFrameSize;
        config.outputSize = 2;  // Binary classification
        config.sampleRate = 16000.0f;
        config.confidenceThreshold = 0.7f;
        
        mAiInference = createAiInference(config);
        if (!mAiInference || !mAiInference->isReady()) {
            throw std::runtime_error("Failed to initialize AI inference engine");
        }
        
        // Warmup the AI inference engine
        mAiInference->warmup(5);
        
        // Initialize thread pool
        mThreadPool = std::make_unique<RealtimeThreadPool>(
            RealtimeThreadPool::Priority::Low, 
            kFrameSize
        );
        
        std::cout << "AudioThreadPoolDemo initialized:" << std::endl;
        std::cout << "  Thread pool size: " << mThreadPool->getThreadCount() << std::endl;
        std::cout << "  Frame size: " << kFrameSize << " samples" << std::endl;
        std::cout << "  Processing time: 20ms frames" << std::endl;
        std::cout << "  AI inference ready: " << (mAiInference->isReady() ? "Yes" : "No") << std::endl;
    }
    
    ~AudioThreadPoolDemo()
    {
        stop();
    }
    
    void run(int durationSeconds = 10)
    {
        std::cout << "\n=== Starting Audio Processing Demo ===" << std::endl;
        std::cout << "Duration: " << durationSeconds << " seconds" << std::endl;
        
        // Start thread pool
        mThreadPool->start(mRingBuffer.get(), mAiInference.get(), 20); // 20ms intervals
        
        // Start audio simulation thread
        auto audioSimulationThread = std::thread(&AudioThreadPoolDemo::simulateAudioThread, this);
        
        // Monitor performance
        auto monitoringThread = std::thread(&AudioThreadPoolDemo::monitorPerformance, this);
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        
        // Stop everything
        std::cout << "\nStopping demo..." << std::endl;
        mStopFlag.store(true);
        
        // Wait for threads to finish
        audioSimulationThread.join();
        monitoringThread.join();
        
        // Stop thread pool
        mThreadPool->stop();
        
        // Print final statistics
        printFinalStatistics();
    }
    
    void stop()
    {
        mStopFlag.store(true);
        if (mThreadPool) {
            mThreadPool->stop();
        }
    }

private:
    static constexpr int kFrameSize = 320;      // 20ms at 16kHz
    static constexpr int kSampleRate = 16000;   // Target sample rate
    static constexpr float kMaxAmplitude = 0.5f; // Maximum test amplitude
    
    std::unique_ptr<RingBuffer<float, 2048>> mRingBuffer;
    std::unique_ptr<AiInference> mAiInference;
    std::unique_ptr<RealtimeThreadPool> mThreadPool;
    std::atomic<bool> mStopFlag;
    
    // Audio simulation parameters
    std::atomic<int> mAudioSamplesGenerated{0};
    std::atomic<int> mAudioFramesGenerated{0};
    
    /**
     * @brief Simulate audio thread that generates and processes audio data
     * 
     * This simulates a real audio callback that would run at high priority
     * and needs to maintain real-time constraints.
     */
    void simulateAudioThread()
    {
        std::cout << "Audio simulation thread started" << std::endl;
        
        // Set up audio generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> noiseDist(-0.1f, 0.1f);
        std::uniform_real_distribution<float> signalDist(-kMaxAmplitude, kMaxAmplitude);
        std::uniform_int_distribution<int> burstDist(0, 1000);
        
        // Generate different types of test signals
        enum class SignalType { Silence, Noise, Tone, Burst, Speech };
        auto signalTypes = {SignalType::Silence, SignalType::Noise, SignalType::Tone, 
                           SignalType::Burst, SignalType::Speech};
        auto signalTypeIt = signalTypes.begin();
        
        float phase = 0.0f;
        const float toneFreq = 440.0f; // A4 note
        const float phaseIncrement = 2.0f * M_PI * toneFreq / kSampleRate;
        
        int signalSwitchCounter = 0;
        const int switchInterval = kSampleRate * 2; // Switch signal type every 2 seconds
        
        auto lastTime = std::chrono::high_resolution_clock::now();
        const auto targetInterval = std::chrono::microseconds(1000000 / (kSampleRate / 64)); // 64 samples per callback
        
        while (!mStopFlag.load()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            
            // Generate 64 samples (simulating audio callback)
            constexpr int callbackSize = 64;
            std::vector<float> audioSamples(callbackSize);
            
            // Switch signal type periodically
            if (signalSwitchCounter >= switchInterval) {
                signalSwitchCounter = 0;
                ++signalTypeIt;
                if (signalTypeIt == signalTypes.end()) {
                    signalTypeIt = signalTypes.begin();
                }
                std::cout << "Switching to signal type: " << static_cast<int>(*signalTypeIt) << std::endl;
            }
            
            // Generate samples based on current signal type
            for (int i = 0; i < callbackSize; ++i) {
                float sample = 0.0f;
                
                switch (*signalTypeIt) {
                    case SignalType::Silence:
                        sample = noiseDist(gen) * 0.01f; // Very quiet noise
                        break;
                    
                    case SignalType::Noise:
                        sample = noiseDist(gen);
                        break;
                    
                    case SignalType::Tone:
                        sample = std::sin(phase) * 0.3f + noiseDist(gen) * 0.05f;
                        phase += phaseIncrement;
                        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
                        break;
                    
                    case SignalType::Burst:
                        if (burstDist(gen) < 50) { // 5% chance of burst
                            sample = signalDist(gen);
                        } else {
                            sample = noiseDist(gen) * 0.1f;
                        }
                        break;
                    
                    case SignalType::Speech:
                        // Simulate speech-like signal with varying amplitude and frequency
                        float speechEnvelope = std::abs(std::sin(phase * 0.1f)) * 0.7f;
                        sample = (std::sin(phase) + 0.3f * std::sin(phase * 3.0f)) * speechEnvelope;
                        sample += noiseDist(gen) * 0.02f;
                        phase += phaseIncrement * (1.0f + 0.3f * std::sin(phase * 0.05f));
                        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
                        break;
                }
                
                audioSamples[i] = sample;
            }
            
            // Push samples to ring buffer (simulating decimated audio data)
            for (float sample : audioSamples) {
                if (!mRingBuffer->push(sample)) {
                    // Buffer overflow - in a real system, this would be problematic
                    std::cerr << "Ring buffer overflow!" << std::endl;
                    break;
                }
                mAudioSamplesGenerated.fetch_add(1);
            }
            
            signalSwitchCounter += callbackSize;
            
            // Maintain real-time timing
            auto nextTime = lastTime + targetInterval;
            if (currentTime < nextTime) {
                std::this_thread::sleep_until(nextTime);
            }
            lastTime = nextTime;
        }
        
        std::cout << "Audio simulation thread finished" << std::endl;
        std::cout << "Total samples generated: " << mAudioSamplesGenerated.load() << std::endl;
    }
    
    /**
     * @brief Monitor thread pool and AI inference performance
     */
    void monitorPerformance()
    {
        std::cout << "Performance monitoring thread started" << std::endl;
        
        auto lastMonitorTime = std::chrono::steady_clock::now();
        auto lastFrameCount = 0ULL;
        
        while (!mStopFlag.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastMonitorTime);
            
            // Get thread pool statistics
            auto threadPoolStats = mThreadPool->getStatistics();
            auto currentFrameCount = threadPoolStats.framesProcessed.load();
            auto frameRate = (currentFrameCount - lastFrameCount) / static_cast<double>(elapsed.count());
            
            // Get AI inference statistics
            auto aiStats = mAiInference->getStatistics();
            
            // Print performance report
            std::cout << "\n--- Performance Report ---" << std::endl;
            std::cout << "Thread Pool:" << std::endl;
            std::cout << "  Frames processed: " << currentFrameCount << std::endl;
            std::cout << "  Processing rate: " << std::fixed << std::setprecision(1) 
                      << frameRate << " frames/sec" << std::endl;
            std::cout << "  Average processing time: " << std::fixed << std::setprecision(3)
                      << threadPoolStats.averageProcessingTimeMs.load() << " ms" << std::endl;
            std::cout << "  CPU usage: " << std::fixed << std::setprecision(1)
                      << threadPoolStats.cpuUsagePercent.load() << "%" << std::endl;
            std::cout << "  Dropped frames: " << threadPoolStats.droppedFrames.load() << std::endl;
            std::cout << "  Queue size: " << mThreadPool->getQueueSize() << std::endl;
            
            std::cout << "AI Inference:" << std::endl;
            std::cout << "  Total inferences: " << aiStats.totalInferences.load() << std::endl;
            std::cout << "  Success rate: " << std::fixed << std::setprecision(1)
                      << (aiStats.totalInferences.load() > 0 ? 
                         100.0 * aiStats.successfulInferences.load() / aiStats.totalInferences.load() : 0.0)
                      << "%" << std::endl;
            std::cout << "  Average confidence: " << std::fixed << std::setprecision(3)
                      << aiStats.averageConfidence.load() << std::endl;
            std::cout << "  Average inference time: " << std::fixed << std::setprecision(3)
                      << aiStats.averageProcessingTimeMs.load() << " ms" << std::endl;
            
            std::cout << "Ring Buffer:" << std::endl;
            std::cout << "  Current size: " << mRingBuffer->size() << " / " << mRingBuffer->capacity() << std::endl;
            std::cout << "  Fill level: " << std::fixed << std::setprecision(1)
                      << (100.0 * mRingBuffer->size() / mRingBuffer->capacity()) << "%" << std::endl;
            
            lastMonitorTime = currentTime;
            lastFrameCount = currentFrameCount;
        }
        
        std::cout << "Performance monitoring thread finished" << std::endl;
    }
    
    /**
     * @brief Print final performance statistics
     */
    void printFinalStatistics()
    {
        std::cout << "\n=== Final Performance Statistics ===" << std::endl;
        
        auto threadPoolStats = mThreadPool->getStatistics();
        auto aiStats = mAiInference->getStatistics();
        
        std::cout << "Thread Pool Performance:" << std::endl;
        std::cout << "  Total frames processed: " << threadPoolStats.framesProcessed.load() << std::endl;
        std::cout << "  Total processing time: " << (threadPoolStats.totalProcessingTimeUs.load() / 1000.0) << " ms" << std::endl;
        std::cout << "  Average processing time: " << std::fixed << std::setprecision(3)
                  << threadPoolStats.averageProcessingTimeMs.load() << " ms/frame" << std::endl;
        std::cout << "  Peak CPU usage: " << std::fixed << std::setprecision(1)
                  << threadPoolStats.cpuUsagePercent.load() << "%" << std::endl;
        std::cout << "  Dropped frames: " << threadPoolStats.droppedFrames.load() << std::endl;
        
        if (threadPoolStats.framesProcessed.load() > 0) {
            double successRate = 100.0 * (threadPoolStats.framesProcessed.load() - threadPoolStats.droppedFrames.load()) 
                                / threadPoolStats.framesProcessed.load();
            std::cout << "  Success rate: " << std::fixed << std::setprecision(1) << successRate << "%" << std::endl;
        }
        
        std::cout << "\nAI Inference Performance:" << std::endl;
        std::cout << "  Total inferences: " << aiStats.totalInferences.load() << std::endl;
        std::cout << "  Successful inferences: " << aiStats.successfulInferences.load() << std::endl;
        std::cout << "  Failed inferences: " << aiStats.failedInferences.load() << std::endl;
        
        if (aiStats.totalInferences.load() > 0) {
            double successRate = 100.0 * aiStats.successfulInferences.load() / aiStats.totalInferences.load();
            std::cout << "  AI success rate: " << std::fixed << std::setprecision(1) << successRate << "%" << std::endl;
        }
        
        std::cout << "  Average inference time: " << std::fixed << std::setprecision(3)
                  << aiStats.averageProcessingTimeMs.load() << " ms" << std::endl;
        std::cout << "  Average confidence: " << std::fixed << std::setprecision(3)
                  << aiStats.averageConfidence.load() << std::endl;
        
        std::cout << "\nAudio Generation:" << std::endl;
        std::cout << "  Total samples generated: " << mAudioSamplesGenerated.load() << std::endl;
        std::cout << "  Expected frames: " << (mAudioSamplesGenerated.load() / kFrameSize) << std::endl;
        
        // Performance evaluation
        std::cout << "\n=== Performance Evaluation ===" << std::endl;
        bool performanceGood = true;
        
        if (threadPoolStats.averageProcessingTimeMs.load() > 15.0) {
            std::cout << "⚠️  High processing time detected" << std::endl;
            performanceGood = false;
        }
        
        if (threadPoolStats.cpuUsagePercent.load() > 50.0) {
            std::cout << "⚠️  High CPU usage detected" << std::endl;
            performanceGood = false;
        }
        
        if (threadPoolStats.droppedFrames.load() > (threadPoolStats.framesProcessed.load() * 0.05)) {
            std::cout << "⚠️  High frame drop rate detected" << std::endl;
            performanceGood = false;
        }
        
        if (performanceGood) {
            std::cout << "✅ Performance is within acceptable limits" << std::endl;
        }
        
        std::cout << "\nDemo completed successfully!" << std::endl;
    }
};

int main(int argc, char* argv[])
{
    try {
        int duration = 10; // Default duration
        
        if (argc > 1) {
            duration = std::atoi(argv[1]);
            if (duration <= 0 || duration > 300) {
                std::cout << "Duration must be between 1 and 300 seconds" << std::endl;
                return 1;
            }
        }
        
        std::cout << "RealtimeThreadPool Demo" << std::endl;
        std::cout << "======================" << std::endl;
        std::cout << "This demo simulates real-time audio processing with AI inference" << std::endl;
        std::cout << "running in a lower-priority thread pool." << std::endl;
        std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << std::endl;
        
        AudioThreadPoolDemo demo;
        demo.run(duration);
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 