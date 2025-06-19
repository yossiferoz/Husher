#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>

#include "RealtimeThreadPool.h"
#include "AiInference.h"
#include "RingBuffer.h"

using namespace KhDetector;

class RealtimeThreadPoolTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Create AI inference engine
        aiConfig = createDefaultModelConfig();
        aiConfig.inputSize = 320;  // 20ms at 16kHz
        aiInference = createAiInference(aiConfig);
        ASSERT_NE(aiInference, nullptr);
        
        // Create ring buffer
        ringBuffer = std::make_unique<RingBuffer<float, 2048>>();
        
        // Create thread pool
        threadPool = std::make_unique<RealtimeThreadPool>(
            RealtimeThreadPool::Priority::Low, 
            320
        );
    }
    
    void TearDown() override 
    {
        if (threadPool) {
            threadPool->stop();
        }
    }
    
    void fillRingBufferWithTestData(int numFrames) 
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        
        for (int frame = 0; frame < numFrames; ++frame) {
            for (int sample = 0; sample < 320; ++sample) {
                float value = dist(gen);
                ringBuffer->push(value);
            }
        }
    }
    
    AiInference::ModelConfig aiConfig;
    std::unique_ptr<AiInference> aiInference;
    std::unique_ptr<RingBuffer<float, 2048>> ringBuffer;
    std::unique_ptr<RealtimeThreadPool> threadPool;
};

TEST_F(RealtimeThreadPoolTest, ConstructorAndDestructor)
{
    // Test basic construction
    auto pool = std::make_unique<RealtimeThreadPool>();
    EXPECT_FALSE(pool->isRunning());
    EXPECT_GT(pool->getThreadCount(), 0);
    EXPECT_LE(pool->getThreadCount(), std::thread::hardware_concurrency());
    
    // Test custom thread count
    auto customPool = std::make_unique<RealtimeThreadPool>(2);
    EXPECT_EQ(customPool->getThreadCount(), 2);
    
    // Test that destructor properly stops threads
    customPool.reset();
    // If we get here without hanging, destructor worked correctly
}

TEST_F(RealtimeThreadPoolTest, StartAndStop)
{
    EXPECT_FALSE(threadPool->isRunning());
    
    // Start the thread pool
    threadPool->start(ringBuffer.get(), aiInference.get());
    EXPECT_TRUE(threadPool->isRunning());
    
    // Stop the thread pool
    threadPool->stop();
    EXPECT_FALSE(threadPool->isRunning());
    
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; ++i) {
        threadPool->start(ringBuffer.get(), aiInference.get());
        EXPECT_TRUE(threadPool->isRunning());
        threadPool->stop();
        EXPECT_FALSE(threadPool->isRunning());
    }
}

TEST_F(RealtimeThreadPoolTest, InvalidParameters)
{
    // Test with null ring buffer
    threadPool->start(nullptr, aiInference.get());
    EXPECT_FALSE(threadPool->isRunning());
    
    // Test with null AI inference
    threadPool->start(ringBuffer.get(), nullptr);
    EXPECT_FALSE(threadPool->isRunning());
    
    // Test with both null
    threadPool->start(nullptr, nullptr);
    EXPECT_FALSE(threadPool->isRunning());
}

TEST_F(RealtimeThreadPoolTest, TaskSubmission)
{
    std::atomic<int> taskCounter{0};
    
    threadPool->start(ringBuffer.get(), aiInference.get());
    
    // Submit some tasks
    for (int i = 0; i < 10; ++i) {
        bool submitted = threadPool->submitTask([&taskCounter]() {
            taskCounter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        EXPECT_TRUE(submitted);
    }
    
    // Wait for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_EQ(taskCounter.load(), 10);
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, QueueOverflow)
{
    threadPool->start(ringBuffer.get(), aiInference.get());
    
    // Submit tasks that will block to fill the queue
    std::atomic<bool> blockTasks{true};
    
    // Fill the queue beyond capacity
    int successfulSubmissions = 0;
    for (int i = 0; i < 100; ++i) {
        bool submitted = threadPool->submitTask([&blockTasks]() {
            while (blockTasks.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        if (submitted) {
            successfulSubmissions++;
        }
    }
    
    // Should have some successful submissions but not all
    EXPECT_GT(successfulSubmissions, 0);
    EXPECT_LT(successfulSubmissions, 100);
    
    // Unblock tasks
    blockTasks.store(false);
    
    // Wait for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, AIInferenceProcessing)
{
    // Fill ring buffer with test data
    fillRingBufferWithTestData(5);  // 5 frames of 320 samples each
    
    threadPool->start(ringBuffer.get(), aiInference.get(), 10); // 10ms interval for faster testing
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check statistics
    auto stats = threadPool->getStatistics();
    EXPECT_GT(stats.framesProcessed.load(), 0);
    
    // Check AI inference statistics
    auto aiStats = aiInference->getStatistics();
    EXPECT_GT(aiStats.totalInferences.load(), 0);
    EXPECT_GT(aiStats.successfulInferences.load(), 0);
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, ProcessingInterval)
{
    fillRingBufferWithTestData(10);
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Start with 50ms interval
    threadPool->start(ringBuffer.get(), aiInference.get(), 50);
    
    // Wait for a few processing cycles
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto stats = threadPool->getStatistics();
    auto framesProcessed = stats.framesProcessed.load();
    
    // Should have processed approximately 200ms / 50ms = 4 frames
    EXPECT_GE(framesProcessed, 2);  // At least 2 frames
    EXPECT_LE(framesProcessed, 6);  // But not too many
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, ConcurrentAccess)
{
    threadPool->start(ringBuffer.get(), aiInference.get());
    
    std::atomic<int> producerCount{0};
    std::atomic<int> consumerCount{0};
    
    // Producer thread - adds data to ring buffer
    std::thread producer([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        
        for (int i = 0; i < 1000; ++i) {
            float value = dist(gen);
            if (ringBuffer->push(value)) {
                producerCount.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Consumer thread - submits tasks
    std::thread consumer([&]() {
        for (int i = 0; i < 100; ++i) {
            bool submitted = threadPool->submitTask([&consumerCount]() {
                consumerCount.fetch_add(1);
            });
            if (submitted) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Wait for processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_GT(producerCount.load(), 0);
    EXPECT_GT(consumerCount.load(), 0);
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, Statistics)
{
    threadPool->start(ringBuffer.get(), aiInference.get());
    
    // Initially, statistics should be zero
    auto stats = threadPool->getStatistics();
    EXPECT_EQ(stats.framesProcessed.load(), 0);
    EXPECT_EQ(stats.droppedFrames.load(), 0);
    
    // Add some data and wait for processing
    fillRingBufferWithTestData(3);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check updated statistics
    stats = threadPool->getStatistics();
    EXPECT_GT(stats.framesProcessed.load(), 0);
    EXPECT_GE(stats.averageProcessingTimeMs.load(), 0.0);
    
    // Reset statistics
    threadPool->resetStatistics();
    stats = threadPool->getStatistics();
    EXPECT_EQ(stats.framesProcessed.load(), 0);
    EXPECT_EQ(stats.droppedFrames.load(), 0);
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, ThreadPriority)
{
    // Test different priority levels
    std::vector<RealtimeThreadPool::Priority> priorities = {
        RealtimeThreadPool::Priority::Low,
        RealtimeThreadPool::Priority::Normal,
        RealtimeThreadPool::Priority::High
    };
    
    for (auto priority : priorities) {
        auto pool = std::make_unique<RealtimeThreadPool>(1, priority, 320);
        
        pool->start(ringBuffer.get(), aiInference.get());
        EXPECT_TRUE(pool->isRunning());
        
        // Add some work
        fillRingBufferWithTestData(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        pool->stop();
        EXPECT_FALSE(pool->isRunning());
    }
}

TEST_F(RealtimeThreadPoolTest, EmptyRingBuffer)
{
    // Start with empty ring buffer
    threadPool->start(ringBuffer.get(), aiInference.get(), 10); // Fast interval
    
    // Wait and check that it doesn't crash
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    auto stats = threadPool->getStatistics();
    // Should be zero or very low processing since buffer is empty
    EXPECT_LE(stats.framesProcessed.load(), 1);
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, RapidStartStop)
{
    // Test rapid start/stop cycles
    for (int i = 0; i < 10; ++i) {
        threadPool->start(ringBuffer.get(), aiInference.get());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        threadPool->stop();
    }
    
    // Should not crash or hang
    EXPECT_FALSE(threadPool->isRunning());
}

TEST_F(RealtimeThreadPoolTest, LongRunningStressTest)
{
    threadPool->start(ringBuffer.get(), aiInference.get(), 20);
    
    // Run for a longer period with continuous data
    std::thread dataFeeder([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        
        for (int i = 0; i < 500; ++i) {  // 500 iterations
            for (int sample = 0; sample < 32; ++sample) {  // 32 samples per iteration
                float value = dist(gen);
                ringBuffer->push(value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    dataFeeder.join();
    
    // Wait for processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto stats = threadPool->getStatistics();
    EXPECT_GT(stats.framesProcessed.load(), 0);
    EXPECT_LT(stats.cpuUsagePercent.load(), 100.0);  // Should not max out CPU
    
    threadPool->stop();
}

TEST_F(RealtimeThreadPoolTest, PerformanceBenchmark)
{
    // This test measures performance characteristics
    fillRingBufferWithTestData(100);  // Lots of data
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    threadPool->start(ringBuffer.get(), aiInference.get(), 1); // 1ms interval for stress test
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    threadPool->stop();
    
    auto stats = threadPool->getStatistics();
    
    // Performance validation
    if (stats.framesProcessed.load() > 0) {
        double avgProcessingTime = stats.averageProcessingTimeMs.load();
        double cpuUsage = stats.cpuUsagePercent.load();
        
        // These are reasonable bounds for a stub implementation
        EXPECT_LT(avgProcessingTime, 10.0);  // Should be less than 10ms average
        EXPECT_LT(cpuUsage, 50.0);           // Should not use more than 50% CPU
        
        std::cout << "Performance Results:" << std::endl;
        std::cout << "  Frames processed: " << stats.framesProcessed.load() << std::endl;
        std::cout << "  Average processing time: " << avgProcessingTime << " ms" << std::endl;
        std::cout << "  CPU usage: " << cpuUsage << "%" << std::endl;
        std::cout << "  Dropped frames: " << stats.droppedFrames.load() << std::endl;
    }
} 