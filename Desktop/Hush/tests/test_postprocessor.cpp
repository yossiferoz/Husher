#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>

#include "PostProcessor.h"

using namespace KhDetector;

class PostProcessorTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Default configuration for most tests
        config = PostProcessor::Config{};
        config.medianFilterSize = 5;
        config.threshold = 0.6f;
        config.enableHysteresis = false;
        config.minHitDuration = 3;
        config.maxHitDuration = 100;
        config.enableDebounce = true;
        config.debounceFrames = 2;
        
        processor = std::make_unique<PostProcessor>(config);
    }
    
    void TearDown() override 
    {
        processor.reset();
    }
    
    // Helper function to process a vector of confidence values
    std::vector<float> processConfidenceVector(const std::vector<float>& confidences) 
    {
        std::vector<float> smoothed;
        smoothed.reserve(confidences.size());
        
        for (float confidence : confidences) {
            float result = processor->processConfidence(confidence);
            smoothed.push_back(result);
        }
        
        return smoothed;
    }
    
    // Helper to create synthetic confidence vectors
    std::vector<float> createSyntheticConfidences(const std::string& pattern) 
    {
        std::vector<float> confidences;
        
        if (pattern == "simple_hit") {
            // Simple hit pattern: low -> high -> low
            confidences = {0.1f, 0.2f, 0.3f, 0.7f, 0.8f, 0.9f, 0.8f, 0.7f, 0.3f, 0.2f, 0.1f};
            
        } else if (pattern == "noisy_hit") {
            // Noisy hit pattern with outliers
            confidences = {0.1f, 0.9f, 0.2f, 0.7f, 0.1f, 0.8f, 0.9f, 0.2f, 0.8f, 0.1f, 0.2f};
            
        } else if (pattern == "sustained_hit") {
            // Long sustained hit
            confidences.resize(50, 0.1f);
            for (int i = 10; i < 40; ++i) {
                confidences[i] = 0.8f;
            }
            
        } else if (pattern == "multiple_hits") {
            // Multiple separate hits
            confidences = {0.1f, 0.8f, 0.9f, 0.8f, 0.1f, 0.1f, 0.1f, 0.7f, 0.8f, 0.7f, 0.1f};
            
        } else if (pattern == "false_positive") {
            // Very short spike that should be filtered out
            confidences = {0.1f, 0.2f, 0.9f, 0.1f, 0.2f, 0.1f};
            
        } else if (pattern == "gradual_rise") {
            // Gradual rise and fall
            for (int i = 0; i < 20; ++i) {
                float t = i / 19.0f;
                confidences.push_back(0.1f + 0.8f * std::sin(t * M_PI));
            }
            
        } else if (pattern == "random_noise") {
            // Random noise around threshold
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(0.5f, 0.7f);
            
            for (int i = 0; i < 20; ++i) {
                confidences.push_back(dist(gen));
            }
        }
        
        return confidences;
    }
    
    PostProcessor::Config config;
    std::unique_ptr<PostProcessor> processor;
};

TEST_F(PostProcessorTest, BasicConstruction)
{
    EXPECT_FALSE(processor->hasHit());
    EXPECT_EQ(processor->getCurrentSmoothedConfidence(), 0.0f);
    EXPECT_TRUE(processor->isEnabled());
    
    auto stats = processor->getStatistics();
    EXPECT_EQ(stats.totalFramesProcessed.load(), 0);
    EXPECT_EQ(stats.totalHits.load(), 0);
}

TEST_F(PostProcessorTest, ConfigValidation)
{
    // Test automatic config correction
    PostProcessor::Config invalidConfig;
    invalidConfig.medianFilterSize = 2;  // Even number, should be corrected
    invalidConfig.threshold = 1.5f;      // Out of range, should be clamped
    invalidConfig.minHitDuration = 0;    // Invalid, should be corrected
    
    auto testProcessor = std::make_unique<PostProcessor>(invalidConfig);
    auto correctedConfig = testProcessor->getConfig();
    
    EXPECT_EQ(correctedConfig.medianFilterSize, 3);  // Should be made odd
    EXPECT_EQ(correctedConfig.threshold, 1.0f);      // Should be clamped
    EXPECT_GE(correctedConfig.minHitDuration, 1);    // Should be at least 1
}

TEST_F(PostProcessorTest, MedianFilterBasic)
{
    // Test median filtering with simple patterns
    auto confidences = createSyntheticConfidences("noisy_hit");
    auto smoothed = processConfidenceVector(confidences);
    
    // Smoothed values should be less noisy
    EXPECT_GT(smoothed.size(), 0);
    
    // Check that extreme outliers are reduced
    float maxRaw = *std::max_element(confidences.begin(), confidences.end());
    float maxSmoothed = *std::max_element(smoothed.begin(), smoothed.end());
    
    // With median filtering, the max should be somewhat reduced due to outlier suppression
    EXPECT_LE(maxSmoothed, maxRaw);
}

TEST_F(PostProcessorTest, SimpleHitDetection)
{
    auto confidences = createSyntheticConfidences("simple_hit");
    
    bool hitDetected = false;
    int hitStartIndex = -1;
    int hitEndIndex = -1;
    
    for (size_t i = 0; i < confidences.size(); ++i) {
        processor->processConfidence(confidences[i]);
        
        bool currentHit = processor->hasHit();
        if (currentHit && !hitDetected) {
            hitDetected = true;
            hitStartIndex = static_cast<int>(i);
        } else if (!currentHit && hitDetected && hitEndIndex == -1) {
            hitEndIndex = static_cast<int>(i);
        }
    }
    
    EXPECT_TRUE(hitDetected);
    EXPECT_GT(hitStartIndex, 0);  // Should not trigger immediately
    EXPECT_GT(hitEndIndex, hitStartIndex);
    
    auto stats = processor->getStatistics();
    EXPECT_GT(stats.totalHits.load(), 0);
    EXPECT_GT(stats.totalFramesProcessed.load(), 0);
}

TEST_F(PostProcessorTest, MinimumHitDuration)
{
    // Test that short spikes are filtered out
    auto confidences = createSyntheticConfidences("false_positive");
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    // Should not trigger hit due to minimum duration requirement
    EXPECT_FALSE(processor->hasHit());
    
    auto stats = processor->getStatistics();
    EXPECT_GT(stats.falsePositives.load(), 0);  // Should count as false positive
}

TEST_F(PostProcessorTest, SustainedHit)
{
    auto confidences = createSyntheticConfidences("sustained_hit");
    
    bool hitDetected = false;
    int hitDuration = 0;
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
        
        if (processor->hasHit()) {
            hitDetected = true;
            hitDuration++;
        }
    }
    
    EXPECT_TRUE(hitDetected);
    EXPECT_GT(hitDuration, config.minHitDuration);
    
    auto lastEvent = processor->getLastHitEvent();
    EXPECT_GT(lastEvent.hitDurationFrames, config.minHitDuration);
    EXPECT_GT(lastEvent.peakConfidence, config.threshold);
}

TEST_F(PostProcessorTest, MultipleHits)
{
    auto confidences = createSyntheticConfidences("multiple_hits");
    
    int hitCount = 0;
    bool wasHit = false;
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
        
        bool currentHit = processor->hasHit();
        if (currentHit && !wasHit) {
            hitCount++;
        }
        wasHit = currentHit;
    }
    
    // Should detect multiple separate hits
    EXPECT_GT(hitCount, 1);
    
    auto stats = processor->getStatistics();
    EXPECT_GE(stats.totalHits.load(), hitCount);
}

TEST_F(PostProcessorTest, HysteresisThresholding)
{
    // Test hysteresis functionality
    config.enableHysteresis = true;
    config.hysteresisHigh = 0.7f;
    config.hysteresisLow = 0.5f;
    config.minHitDuration = 1;  // Immediate response for this test
    
    auto hysteresisProcessor = std::make_unique<PostProcessor>(config);
    
    // Test sequence: low -> high (should trigger) -> medium (should stay) -> low (should end)
    std::vector<float> sequence = {0.3f, 0.8f, 0.6f, 0.6f, 0.4f};
    
    std::vector<bool> hitStates;
    for (float confidence : sequence) {
        hysteresisProcessor->processConfidence(confidence);
        hitStates.push_back(hysteresisProcessor->hasHit());
    }
    
    // Should trigger on high value and stay active due to hysteresis
    EXPECT_FALSE(hitStates[0]);  // Low value
    EXPECT_TRUE(hitStates[1]);   // High value - trigger
    EXPECT_TRUE(hitStates[2]);   // Medium value - stay due to hysteresis
    EXPECT_TRUE(hitStates[3]);   // Medium value - stay due to hysteresis
    EXPECT_FALSE(hitStates[4]);  // Low value - end hit
}

TEST_F(PostProcessorTest, DebounceFunction)
{
    // Test debouncing with rapid hits
    config.enableDebounce = true;
    config.debounceFrames = 3;
    config.minHitDuration = 1;
    
    auto debounceProcessor = std::make_unique<PostProcessor>(config);
    
    // Sequence: hit -> gap -> immediate hit (should be debounced)
    std::vector<float> sequence = {0.8f, 0.1f, 0.8f, 0.1f};
    
    int hitCount = 0;
    bool wasHit = false;
    
    for (float confidence : sequence) {
        debounceProcessor->processConfidence(confidence);
        
        bool currentHit = debounceProcessor->hasHit();
        if (currentHit && !wasHit) {
            hitCount++;
        }
        wasHit = currentHit;
    }
    
    // Should only detect one hit due to debouncing
    EXPECT_EQ(hitCount, 1);
    
    auto stats = debounceProcessor->getStatistics();
    EXPECT_GT(stats.debouncedHits.load(), 0);
}

TEST_F(PostProcessorTest, MaxHitDuration)
{
    // Test maximum hit duration auto-reset
    config.maxHitDuration = 5;
    config.minHitDuration = 1;
    
    auto maxDurationProcessor = std::make_unique<PostProcessor>(config);
    
    // Sustained high confidence for longer than max duration
    std::vector<float> longHit(10, 0.8f);
    
    bool hitEnded = false;
    for (size_t i = 0; i < longHit.size(); ++i) {
        maxDurationProcessor->processConfidence(longHit[i]);
        
        if (i > config.maxHitDuration && !maxDurationProcessor->hasHit()) {
            hitEnded = true;
            break;
        }
    }
    
    EXPECT_TRUE(hitEnded);  // Hit should auto-reset after max duration
}

TEST_F(PostProcessorTest, StatisticsAccuracy)
{
    auto confidences = createSyntheticConfidences("multiple_hits");
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    auto stats = processor->getStatistics();
    
    EXPECT_EQ(stats.totalFramesProcessed.load(), confidences.size());
    EXPECT_GT(stats.averageConfidence.load(), 0.0);
    EXPECT_GT(stats.averageSmoothedConfidence.load(), 0.0);
    EXPECT_GT(stats.peakConfidence.load(), 0.0f);
    
    // Peak should be reasonable
    float maxInput = *std::max_element(confidences.begin(), confidences.end());
    EXPECT_LE(stats.peakConfidence.load(), maxInput);
}

TEST_F(PostProcessorTest, CallbackFunctionality)
{
    bool callbackTriggered = false;
    bool lastHitState = false;
    PostProcessor::HitEvent lastEvent;
    
    processor->setHitCallback([&](bool hitState, const PostProcessor::HitEvent& event) {
        callbackTriggered = true;
        lastHitState = hitState;
        lastEvent = event;
    });
    
    auto confidences = createSyntheticConfidences("simple_hit");
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    EXPECT_TRUE(callbackTriggered);
    EXPECT_TRUE(lastEvent.peakConfidence > 0.0f);
    EXPECT_GT(lastEvent.hitDurationFrames, 0);
}

TEST_F(PostProcessorTest, EnableDisableProcessing)
{
    // Test disabling processing
    processor->setEnabled(false);
    
    std::vector<float> confidences = {0.1f, 0.9f, 0.8f, 0.1f};
    std::vector<float> results;
    
    for (float confidence : confidences) {
        float result = processor->processConfidence(confidence);
        results.push_back(result);
    }
    
    // When disabled, should pass through unchanged
    for (size_t i = 0; i < confidences.size(); ++i) {
        EXPECT_FLOAT_EQ(results[i], confidences[i]);
    }
    
    EXPECT_FALSE(processor->hasHit());  // No hits when disabled
}

TEST_F(PostProcessorTest, ResetFunctionality)
{
    auto confidences = createSyntheticConfidences("simple_hit");
    
    // Process some data
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    // Verify we have some state
    auto statsBefore = processor->getStatistics();
    EXPECT_GT(statsBefore.totalFramesProcessed.load(), 0);
    
    // Reset
    processor->reset();
    
    // Verify reset
    auto statsAfter = processor->getStatistics();
    EXPECT_EQ(statsAfter.totalFramesProcessed.load(), 0);
    EXPECT_EQ(statsAfter.totalHits.load(), 0);
    EXPECT_FALSE(processor->hasHit());
    EXPECT_EQ(processor->getCurrentSmoothedConfidence(), 0.0f);
}

TEST_F(PostProcessorTest, ConfigurationUpdate)
{
    // Test configuration updates
    auto newConfig = config;
    newConfig.threshold = 0.8f;
    newConfig.medianFilterSize = 7;
    
    processor->updateConfig(newConfig);
    
    auto updatedConfig = processor->getConfig();
    EXPECT_FLOAT_EQ(updatedConfig.threshold, 0.8f);
    EXPECT_EQ(updatedConfig.medianFilterSize, 7);
}

TEST_F(PostProcessorTest, FilterHistoryAccess)
{
    std::vector<float> confidences = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    auto history = processor->getFilterHistory();
    EXPECT_EQ(history.size(), config.medianFilterSize);
    
    // Last values should match input (in chronological order)
    for (size_t i = 0; i < std::min(confidences.size(), history.size()); ++i) {
        size_t confIndex = confidences.size() - history.size() + i;
        if (confIndex < confidences.size()) {
            EXPECT_FLOAT_EQ(history[i], confidences[confIndex]);
        }
    }
}

TEST_F(PostProcessorTest, FactoryFunctions)
{
    // Test factory functions
    auto sensitiveProcessor = std::make_unique<PostProcessor>(createSensitiveConfig());
    auto robustProcessor = std::make_unique<PostProcessor>(createRobustConfig());
    auto fastProcessor = std::make_unique<PostProcessor>(createFastConfig());
    
    EXPECT_LT(sensitiveProcessor->getConfig().threshold, robustProcessor->getConfig().threshold);
    EXPECT_LT(fastProcessor->getConfig().medianFilterSize, robustProcessor->getConfig().medianFilterSize);
    EXPECT_FALSE(fastProcessor->getConfig().enableHysteresis);
}

TEST_F(PostProcessorTest, StressTestWithRandomData)
{
    // Stress test with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    const int numSamples = 1000;
    std::vector<float> randomConfidences(numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        randomConfidences[i] = dist(gen);
    }
    
    // Process all samples - should not crash
    for (float confidence : randomConfidences) {
        processor->processConfidence(confidence);
    }
    
    auto stats = processor->getStatistics();
    EXPECT_EQ(stats.totalFramesProcessed.load(), numSamples);
    EXPECT_GE(stats.averageConfidence.load(), 0.0);
    EXPECT_LE(stats.averageConfidence.load(), 1.0);
}

TEST_F(PostProcessorTest, ThreadSafetyBasic)
{
    // Basic thread safety test for read operations
    auto confidences = createSyntheticConfidences("sustained_hit");
    
    std::atomic<bool> stopFlag{false};
    std::atomic<int> readCount{0};
    
    // Reader thread
    std::thread reader([&]() {
        while (!stopFlag.load()) {
            bool hit = processor->hasHit();
            float confidence = processor->getCurrentSmoothedConfidence();
            auto stats = processor->getStatistics();
            (void)hit; (void)confidence; (void)stats;  // Suppress unused warnings
            readCount.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Process data in main thread
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    
    stopFlag.store(true);
    reader.join();
    
    EXPECT_GT(readCount.load(), 0);  // Reader should have done some reads
}

TEST_F(PostProcessorTest, PerformanceBenchmark)
{
    // Performance test
    const int numSamples = 10000;
    std::vector<float> confidences(numSamples, 0.5f);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (float confidence : confidences) {
        processor->processConfidence(confidence);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Should process at least 1000 samples per millisecond (very conservative)
    double samplesPerMicrosecond = static_cast<double>(numSamples) / duration.count();
    EXPECT_GT(samplesPerMicrosecond, 1.0);
    
    std::cout << "PostProcessor Performance: " << samplesPerMicrosecond << " samples/μs" << std::endl;
    std::cout << "Total processing time: " << duration.count() << " μs for " << numSamples << " samples" << std::endl;
} 