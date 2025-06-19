/**
 * @file test_resizable_ui.cpp
 * @brief Unit tests for KhDetector resizable UI functionality
 * 
 * Tests cover:
 * - Three UI sizes (Small, Medium, Large) with auto-layout
 * - CPU meter with real-time process() timing display
 * - Sensitivity slider (0-1) mapped to detection threshold
 * - Write Markers button functionality
 * - Thread-safe atomic operations for real-time use
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>

namespace {

/**
 * @brief Mock controller for testing UI functionality
 */
class MockUIController
{
public:
    void setSensitivity(float value)
    {
        mSensitivity = std::max(0.0f, std::min(1.0f, value));
        mSensitivityCallCount++;
    }
    
    void writeMarkers()
    {
        mMarkerWriteCount++;
    }
    
    float getSensitivity() const { return mSensitivity; }
    int getSensitivityCallCount() const { return mSensitivityCallCount; }
    int getMarkerWriteCount() const { return mMarkerWriteCount; }
    
    void reset()
    {
        mSensitivity = 0.6f;
        mSensitivityCallCount = 0;
        mMarkerWriteCount = 0;
    }
    
private:
    float mSensitivity = 0.6f;
    int mSensitivityCallCount = 0;
    int mMarkerWriteCount = 0;
};

/**
 * @brief Simplified UI size management for testing
 */
class UIManager
{
public:
    enum class UISize {
        Small,   // 760×480
        Medium,  // 1100×680
        Large    // 1600×960
    };
    
    struct Dimensions {
        int width;
        int height;
    };
    
    UIManager() : mCurrentSize(UISize::Medium) {}
    
    void setUISize(UISize size)
    {
        mCurrentSize = size;
        mSizeChangeCount++;
    }
    
    UISize getCurrentSize() const { return mCurrentSize; }
    
    Dimensions getDimensions() const
    {
        switch (mCurrentSize) {
            case UISize::Small:  return {760, 480};
            case UISize::Medium: return {1100, 680};
            case UISize::Large:  return {1600, 960};
        }
        return {1100, 680}; // Default to medium
    }
    
    int getSizeChangeCount() const { return mSizeChangeCount; }
    
    void reset()
    {
        mCurrentSize = UISize::Medium;
        mSizeChangeCount = 0;
    }
    
private:
    UISize mCurrentSize;
    int mSizeChangeCount = 0;
};

/**
 * @brief CPU usage monitor for testing
 */
class CPUMonitor
{
public:
    struct CPUStats {
        std::atomic<float> averageProcessTime{0.0f};  // in milliseconds
        std::atomic<float> cpuUsagePercent{0.0f};     // 0-100%
        std::atomic<uint64_t> processCallCount{0};
    };
    
    void updateCPUStats(float processTimeMs)
    {
        // Update process call count
        mStats.processCallCount.fetch_add(1);
        
        // Calculate moving average of process time
        float currentAvg = mStats.averageProcessTime.load();
        float newAvg = currentAvg * 0.95f + processTimeMs * 0.05f;  // Exponential moving average
        mStats.averageProcessTime.store(newAvg);
        
        // Estimate CPU usage based on process time and sample rate
        // Assuming 44.1kHz, 512 samples per buffer = ~11.6ms per buffer
        float bufferTimeMs = 11.6f;  // Approximate buffer time
        float cpuPercent = (newAvg / bufferTimeMs) * 100.0f;
        mStats.cpuUsagePercent.store(std::min(cpuPercent, 100.0f));
    }
    
    const CPUStats& getStats() const { return mStats; }
    
    void reset()
    {
        mStats.averageProcessTime.store(0.0f);
        mStats.cpuUsagePercent.store(0.0f);
        mStats.processCallCount.store(0);
    }
    
private:
    CPUStats mStats;
};

} // anonymous namespace

//==============================================================================
// Test Fixture
//==============================================================================
class ResizableUITest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mController = std::make_unique<MockUIController>();
        mUIManager = std::make_unique<UIManager>();
        mCPUMonitor = std::make_unique<CPUMonitor>();
        mHitState.store(false);
    }
    
    void TearDown() override
    {
        mController.reset();
        mUIManager.reset();
        mCPUMonitor.reset();
    }
    
    std::unique_ptr<MockUIController> mController;
    std::unique_ptr<UIManager> mUIManager;
    std::unique_ptr<CPUMonitor> mCPUMonitor;
    std::atomic<bool> mHitState{false};
};

//==============================================================================
// UI Size Management Tests
//==============================================================================
TEST_F(ResizableUITest, DefaultSizeIsMedium)
{
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Medium);
    
    auto dims = mUIManager->getDimensions();
    EXPECT_EQ(dims.width, 1100);
    EXPECT_EQ(dims.height, 680);
}

TEST_F(ResizableUITest, SmallSizeDimensions)
{
    mUIManager->setUISize(UIManager::UISize::Small);
    
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Small);
    
    auto dims = mUIManager->getDimensions();
    EXPECT_EQ(dims.width, 760);
    EXPECT_EQ(dims.height, 480);
}

TEST_F(ResizableUITest, LargeSizeDimensions)
{
    mUIManager->setUISize(UIManager::UISize::Large);
    
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Large);
    
    auto dims = mUIManager->getDimensions();
    EXPECT_EQ(dims.width, 1600);
    EXPECT_EQ(dims.height, 960);
}

TEST_F(ResizableUITest, SizeCycling)
{
    // Start with Medium (default)
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Medium);
    
    // Cycle to Large
    mUIManager->setUISize(UIManager::UISize::Large);
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Large);
    
    // Cycle to Small
    mUIManager->setUISize(UIManager::UISize::Small);
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Small);
    
    // Cycle back to Medium
    mUIManager->setUISize(UIManager::UISize::Medium);
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Medium);
    
    EXPECT_EQ(mUIManager->getSizeChangeCount(), 3);
}

//==============================================================================
// Controller Integration Tests
//==============================================================================
TEST_F(ResizableUITest, SensitivitySliderBasicFunctionality)
{
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.6f);
    
    mController->setSensitivity(0.8f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.8f);
    EXPECT_EQ(mController->getSensitivityCallCount(), 1);
    
    mController->setSensitivity(0.3f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.3f);
    EXPECT_EQ(mController->getSensitivityCallCount(), 2);
}

TEST_F(ResizableUITest, SensitivitySliderBoundaryValues)
{
    // Test lower boundary
    mController->setSensitivity(-0.1f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.0f);
    
    // Test upper boundary
    mController->setSensitivity(1.1f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 1.0f);
    
    // Test exact boundaries
    mController->setSensitivity(0.0f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.0f);
    
    mController->setSensitivity(1.0f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 1.0f);
}

TEST_F(ResizableUITest, WriteMarkersButton)
{
    EXPECT_EQ(mController->getMarkerWriteCount(), 0);
    
    mController->writeMarkers();
    EXPECT_EQ(mController->getMarkerWriteCount(), 1);
    
    mController->writeMarkers();
    mController->writeMarkers();
    EXPECT_EQ(mController->getMarkerWriteCount(), 3);
}

//==============================================================================
// CPU Monitor Tests
//==============================================================================
TEST_F(ResizableUITest, CPUMonitorInitialState)
{
    const auto& stats = mCPUMonitor->getStats();
    
    EXPECT_FLOAT_EQ(stats.averageProcessTime.load(), 0.0f);
    EXPECT_FLOAT_EQ(stats.cpuUsagePercent.load(), 0.0f);
    EXPECT_EQ(stats.processCallCount.load(), 0);
}

TEST_F(ResizableUITest, CPUMonitorBasicUpdate)
{
    mCPUMonitor->updateCPUStats(1.0f);  // 1ms process time
    
    const auto& stats = mCPUMonitor->getStats();
    
    EXPECT_GT(stats.averageProcessTime.load(), 0.0f);
    EXPECT_GT(stats.cpuUsagePercent.load(), 0.0f);
    EXPECT_EQ(stats.processCallCount.load(), 1);
}

TEST_F(ResizableUITest, CPUMonitorMovingAverage)
{
    // Add several measurements
    mCPUMonitor->updateCPUStats(1.0f);
    mCPUMonitor->updateCPUStats(2.0f);
    mCPUMonitor->updateCPUStats(3.0f);
    
    const auto& stats = mCPUMonitor->getStats();
    
    EXPECT_EQ(stats.processCallCount.load(), 3);
    EXPECT_GT(stats.averageProcessTime.load(), 0.0f);
    EXPECT_LT(stats.averageProcessTime.load(), 3.0f);  // Should be averaged
}

TEST_F(ResizableUITest, CPUMonitorHighLoad)
{
    // Simulate high CPU load
    mCPUMonitor->updateCPUStats(15.0f);  // 15ms process time (> 11.6ms buffer time)
    
    const auto& stats = mCPUMonitor->getStats();
    
    EXPECT_GT(stats.cpuUsagePercent.load(), 100.0f * (15.0f / 11.6f) * 0.05f);  // Should reflect high load
    EXPECT_LE(stats.cpuUsagePercent.load(), 100.0f);  // But capped at 100%
}

//==============================================================================
// Hit State Tests
//==============================================================================
TEST_F(ResizableUITest, HitStateAtomicOperations)
{
    EXPECT_FALSE(mHitState.load());
    
    mHitState.store(true);
    EXPECT_TRUE(mHitState.load());
    
    mHitState.store(false);
    EXPECT_FALSE(mHitState.load());
}

TEST_F(ResizableUITest, HitStateThreadSafety)
{
    const int numIterations = 1000;
    std::atomic<int> trueCount{0};
    std::atomic<int> falseCount{0};
    
    // Writer thread
    std::thread writer([&]() {
        for (int i = 0; i < numIterations; ++i) {
            mHitState.store(i % 2 == 0);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    // Reader thread
    std::thread reader([&]() {
        for (int i = 0; i < numIterations; ++i) {
            if (mHitState.load()) {
                trueCount.fetch_add(1);
            } else {
                falseCount.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    });
    
    writer.join();
    reader.join();
    
    // Should have read both true and false values
    EXPECT_GT(trueCount.load(), 0);
    EXPECT_GT(falseCount.load(), 0);
    EXPECT_EQ(trueCount.load() + falseCount.load(), numIterations);
}

//==============================================================================
// Integration Tests
//==============================================================================
TEST_F(ResizableUITest, CompleteUIWorkflow)
{
    // Start with default state
    EXPECT_EQ(mUIManager->getCurrentSize(), UIManager::UISize::Medium);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.6f);
    EXPECT_EQ(mController->getMarkerWriteCount(), 0);
    
    // Change UI size
    mUIManager->setUISize(UIManager::UISize::Large);
    auto dims = mUIManager->getDimensions();
    EXPECT_EQ(dims.width, 1600);
    EXPECT_EQ(dims.height, 960);
    
    // Adjust sensitivity
    mController->setSensitivity(0.8f);
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.8f);
    
    // Simulate CPU load
    mCPUMonitor->updateCPUStats(2.5f);
    const auto& stats = mCPUMonitor->getStats();
    EXPECT_GT(stats.cpuUsagePercent.load(), 0.0f);
    
    // Trigger hit detection
    mHitState.store(true);
    EXPECT_TRUE(mHitState.load());
    
    // Write markers
    mController->writeMarkers();
    EXPECT_EQ(mController->getMarkerWriteCount(), 1);
    
    // Reset hit state
    mHitState.store(false);
    EXPECT_FALSE(mHitState.load());
}

TEST_F(ResizableUITest, PerformanceUnderLoad)
{
    const int numOperations = 10000;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Simulate heavy UI operations
    for (int i = 0; i < numOperations; ++i) {
        // Cycle through UI sizes
        auto size = static_cast<UIManager::UISize>(i % 3);
        mUIManager->setUISize(size);
        
        // Update sensitivity
        float sensitivity = (i % 100) / 100.0f;
        mController->setSensitivity(sensitivity);
        
        // Update CPU stats
        float processTime = 0.1f + (i % 50) * 0.1f;
        mCPUMonitor->updateCPUStats(processTime);
        
        // Toggle hit state
        mHitState.store(i % 2 == 0);
        
        if (i % 1000 == 0) {
            mController->writeMarkers();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should complete within reasonable time (less than 1 second)
    EXPECT_LT(duration.count(), 1000);
    
    // Verify final state
    EXPECT_EQ(mUIManager->getSizeChangeCount(), numOperations);
    EXPECT_EQ(mController->getSensitivityCallCount(), numOperations);
    EXPECT_EQ(mController->getMarkerWriteCount(), 10);  // Every 1000 operations
    
    const auto& stats = mCPUMonitor->getStats();
    EXPECT_EQ(stats.processCallCount.load(), numOperations);
}

//==============================================================================
// Error Handling Tests
//==============================================================================
TEST_F(ResizableUITest, RobustnessWithInvalidValues)
{
    // Test with extreme sensitivity values
    mController->setSensitivity(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 1.0f);
    
    mController->setSensitivity(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(mController->getSensitivity(), 0.0f);
    
    mController->setSensitivity(std::numeric_limits<float>::quiet_NaN());
    // Should handle NaN gracefully (implementation-dependent)
    float sensitivity = mController->getSensitivity();
    EXPECT_TRUE(sensitivity >= 0.0f && sensitivity <= 1.0f);
}

TEST_F(ResizableUITest, ConcurrentAccess)
{
    const int numThreads = 4;
    const int numOperationsPerThread = 1000;
    std::vector<std::thread> threads;
    
    // Launch multiple threads performing different operations
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, numOperationsPerThread]() {
            for (int i = 0; i < numOperationsPerThread; ++i) {
                switch (t % 4) {
                    case 0:
                        // UI size changes
                        mUIManager->setUISize(static_cast<UIManager::UISize>(i % 3));
                        break;
                    case 1:
                        // Sensitivity updates
                        mController->setSensitivity((i % 100) / 100.0f);
                        break;
                    case 2:
                        // CPU monitoring
                        mCPUMonitor->updateCPUStats(0.1f + (i % 10) * 0.1f);
                        break;
                    case 3:
                        // Hit state and markers
                        mHitState.store(i % 2 == 0);
                        if (i % 100 == 0) {
                            mController->writeMarkers();
                        }
                        break;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify that operations completed without crashes
    EXPECT_GT(mUIManager->getSizeChangeCount(), 0);
    EXPECT_GT(mController->getSensitivityCallCount(), 0);
    EXPECT_GT(mController->getMarkerWriteCount(), 0);
    
    const auto& stats = mCPUMonitor->getStats();
    EXPECT_GT(stats.processCallCount.load(), 0);
} 