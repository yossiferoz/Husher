#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include <algorithm>

#include "../src/PostProcessor.h"

using namespace KhDetector;

/**
 * @brief Demonstration of PostProcessor functionality
 * 
 * This demo shows various aspects of the PostProcessor:
 * - Median filtering for noise reduction
 * - Threshold detection with hysteresis
 * - Hit duration filtering and debouncing
 * - Different configuration presets
 * - Real-time processing simulation
 */

class PostProcessorDemo
{
public:
    void runAllDemos()
    {
        std::cout << "=== PostProcessor Demonstration ===" << std::endl;
        std::cout << std::endl;
        
        demoBasicFiltering();
        std::cout << std::endl;
        
        demoHitDetection();
        std::cout << std::endl;
        
        demoConfigurationPresets();
        std::cout << std::endl;
        
        demoHysteresisThresholding();
        std::cout << std::endl;
        
        demoDebouncing();
        std::cout << std::endl;
        
        demoRealtimeProcessing();
        std::cout << std::endl;
        
        demoPerformanceComparison();
        std::cout << std::endl;
        
        std::cout << "=== Demo Complete ===" << std::endl;
    }

private:
    void demoBasicFiltering()
    {
        std::cout << "--- Basic Median Filtering Demo ---" << std::endl;
        
        // Create noisy confidence signal with outliers
        std::vector<float> noisySignal = {
            0.1f, 0.9f, 0.2f,  // Outlier spike
            0.3f, 0.4f, 0.5f,  // Gradual rise
            0.6f, 0.1f, 0.7f,  // Another outlier
            0.8f, 0.9f, 0.8f,  // Peak region
            0.7f, 0.2f, 0.6f,  // Outlier dip
            0.5f, 0.4f, 0.3f   // Gradual fall
        };
        
        auto processor = createPostProcessor(0.6f, 5);
        
        std::cout << "Raw vs Filtered Confidence Values:" << std::endl;
        std::cout << "Index | Raw   | Filtered | Difference" << std::endl;
        std::cout << "------|-------|----------|----------" << std::endl;
        
        for (size_t i = 0; i < noisySignal.size(); ++i) {
            float filtered = processor->processConfidence(noisySignal[i]);
            float diff = std::abs(noisySignal[i] - filtered);
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3)
                      << std::setw(5) << noisySignal[i] << " | "
                      << std::setw(8) << filtered << " | "
                      << std::setw(8) << diff << std::endl;
        }
        
        auto stats = processor->getStatistics();
        std::cout << "\nFiltering Statistics:" << std::endl;
        std::cout << "  Frames processed: " << stats.totalFramesProcessed.load() << std::endl;
        std::cout << "  Average raw confidence: " << std::fixed << std::setprecision(3) 
                  << stats.averageConfidence.load() << std::endl;
        std::cout << "  Average filtered confidence: " << std::fixed << std::setprecision(3) 
                  << stats.averageSmoothedConfidence.load() << std::endl;
    }
    
    void demoHitDetection()
    {
        std::cout << "--- Hit Detection Demo ---" << std::endl;
        
        // Create confidence pattern with clear hits
        std::vector<float> confidencePattern = {
            0.1f, 0.2f, 0.3f,           // Baseline
            0.7f, 0.8f, 0.9f, 0.8f,     // First hit (sustained)
            0.3f, 0.2f, 0.1f,           // Return to baseline
            0.1f, 0.2f,                 // Gap
            0.9f,                       // Single spike (should be filtered)
            0.1f, 0.2f,                 // Gap
            0.7f, 0.8f, 0.7f, 0.6f,     // Second hit
            0.2f, 0.1f                  // End
        };
        
        PostProcessor::Config config;
        config.threshold = 0.6f;
        config.medianFilterSize = 3;
        config.minHitDuration = 3;  // Require 3 frames minimum
        config.enableDebounce = true;
        config.debounceFrames = 2;
        
        auto processor = std::make_unique<PostProcessor>(config);
        
        // Set up hit callback
        int hitCount = 0;
        processor->setHitCallback([&](bool hitState, const PostProcessor::HitEvent& event) {
            if (hitState) {
                hitCount++;
                std::cout << "  HIT DETECTED #" << hitCount 
                          << " - Peak: " << std::fixed << std::setprecision(3) << event.peakConfidence
                          << ", Duration: " << event.hitDurationFrames << " frames" << std::endl;
            } else {
                std::cout << "  Hit ended - Final duration: " << event.hitDurationFrames << " frames" << std::endl;
            }
        });
        
        std::cout << "Processing confidence pattern:" << std::endl;
        std::cout << "Frame | Raw   | Filtered | Hit | Status" << std::endl;
        std::cout << "------|-------|----------|-----|-------" << std::endl;
        
        for (size_t i = 0; i < confidencePattern.size(); ++i) {
            float filtered = processor->processConfidence(confidencePattern[i]);
            bool hit = processor->hasHit();
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3)
                      << std::setw(5) << confidencePattern[i] << " | "
                      << std::setw(8) << filtered << " | "
                      << std::setw(3) << (hit ? "YES" : "NO") << " | "
                      << (hit ? "***" : "   ") << std::endl;
        }
        
        auto stats = processor->getStatistics();
        std::cout << "\nHit Detection Statistics:" << std::endl;
        std::cout << "  Total hits detected: " << stats.totalHits.load() << std::endl;
        std::cout << "  False positives filtered: " << stats.falsePositives.load() << std::endl;
        std::cout << "  Debounced hits: " << stats.debouncedHits.load() << std::endl;
    }
    
    void demoConfigurationPresets()
    {
        std::cout << "--- Configuration Presets Demo ---" << std::endl;
        
        // Test signal with moderate noise
        std::vector<float> testSignal = generateTestSignal("moderate_hits");
        
        // Test different presets
        auto sensitiveProcessor = std::make_unique<PostProcessor>(createSensitiveConfig());
        auto robustProcessor = std::make_unique<PostProcessor>(createRobustConfig());
        auto fastProcessor = std::make_unique<PostProcessor>(createFastConfig());
        
        std::cout << "Comparing configuration presets on same signal:" << std::endl;
        std::cout << "Frame | Signal | Sensitive | Robust | Fast | Hits: S|R|F" << std::endl;
        std::cout << "------|--------|-----------|--------|------|----------" << std::endl;
        
        for (size_t i = 0; i < testSignal.size(); ++i) {
            float sensitive = sensitiveProcessor->processConfidence(testSignal[i]);
            float robust = robustProcessor->processConfidence(testSignal[i]);
            float fast = fastProcessor->processConfidence(testSignal[i]);
            
            bool hitS = sensitiveProcessor->hasHit();
            bool hitR = robustProcessor->hasHit();
            bool hitF = fastProcessor->hasHit();
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3)
                      << std::setw(6) << testSignal[i] << " | "
                      << std::setw(9) << sensitive << " | "
                      << std::setw(6) << robust << " | "
                      << std::setw(4) << fast << " | "
                      << (hitS ? "Y" : "N") << "|"
                      << (hitR ? "Y" : "N") << "|"
                      << (hitF ? "Y" : "N") << std::endl;
        }
        
        // Compare final statistics
        std::cout << "\nPreset Comparison:" << std::endl;
        std::cout << "Preset     | Hits | False+ | Threshold | Filter Size" << std::endl;
        std::cout << "-----------|------|--------|-----------|------------" << std::endl;
        
        auto printPresetStats = [](const std::string& name, const PostProcessor* proc) {
            auto stats = proc->getStatistics();
            auto config = proc->getConfig();
            std::cout << std::setw(10) << name << " | "
                      << std::setw(4) << stats.totalHits.load() << " | "
                      << std::setw(6) << stats.falsePositives.load() << " | "
                      << std::fixed << std::setprecision(2)
                      << std::setw(9) << config.threshold << " | "
                      << std::setw(11) << config.medianFilterSize << std::endl;
        };
        
        printPresetStats("Sensitive", sensitiveProcessor.get());
        printPresetStats("Robust", robustProcessor.get());
        printPresetStats("Fast", fastProcessor.get());
    }
    
    void demoHysteresisThresholding()
    {
        std::cout << "--- Hysteresis Thresholding Demo ---" << std::endl;
        
        // Signal that hovers around threshold
        std::vector<float> hoveringSignal = {
            0.3f, 0.4f, 0.5f,           // Below threshold
            0.75f, 0.65f, 0.55f,        // Above high, then between thresholds
            0.65f, 0.55f, 0.65f,        // Oscillating between thresholds
            0.45f, 0.3f                 // Below low threshold
        };
        
        // Compare with and without hysteresis
        PostProcessor::Config normalConfig;
        normalConfig.threshold = 0.6f;
        normalConfig.minHitDuration = 1;
        normalConfig.enableHysteresis = false;
        
        PostProcessor::Config hysteresisConfig = normalConfig;
        hysteresisConfig.enableHysteresis = true;
        hysteresisConfig.hysteresisHigh = 0.7f;
        hysteresisConfig.hysteresisLow = 0.5f;
        
        auto normalProcessor = std::make_unique<PostProcessor>(normalConfig);
        auto hysteresisProcessor = std::make_unique<PostProcessor>(hysteresisConfig);
        
        std::cout << "Comparing normal vs hysteresis thresholding:" << std::endl;
        std::cout << "Frame | Signal | Normal Hit | Hysteresis Hit | Difference" << std::endl;
        std::cout << "------|--------|------------|----------------|----------" << std::endl;
        
        for (size_t i = 0; i < hoveringSignal.size(); ++i) {
            normalProcessor->processConfidence(hoveringSignal[i]);
            hysteresisProcessor->processConfidence(hoveringSignal[i]);
            
            bool normalHit = normalProcessor->hasHit();
            bool hysteresisHit = hysteresisProcessor->hasHit();
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3)
                      << std::setw(6) << hoveringSignal[i] << " | "
                      << std::setw(10) << (normalHit ? "YES" : "NO") << " | "
                      << std::setw(14) << (hysteresisHit ? "YES" : "NO") << " | "
                      << (normalHit != hysteresisHit ? "DIFF" : "SAME") << std::endl;
        }
        
        std::cout << "\nHysteresis reduces false triggering when signal hovers near threshold." << std::endl;
    }
    
    void demoDebouncing()
    {
        std::cout << "--- Debouncing Demo ---" << std::endl;
        
        // Signal with rapid successive hits
        std::vector<float> rapidHitsSignal = {
            0.1f, 0.8f, 0.9f, 0.1f,     // First hit
            0.1f,                       // Short gap (1 frame)
            0.8f, 0.9f, 0.1f,           // Second hit (should be debounced)
            0.1f, 0.1f, 0.1f,           // Longer gap (3 frames)
            0.8f, 0.9f, 0.1f            // Third hit (should be allowed)
        };
        
        PostProcessor::Config noDebounceConfig;
        noDebounceConfig.threshold = 0.6f;
        noDebounceConfig.minHitDuration = 1;
        noDebounceConfig.enableDebounce = false;
        
        PostProcessor::Config debounceConfig = noDebounceConfig;
        debounceConfig.enableDebounce = true;
        debounceConfig.debounceFrames = 2;
        
        auto noDebounceProcessor = std::make_unique<PostProcessor>(noDebounceConfig);
        auto debounceProcessor = std::make_unique<PostProcessor>(debounceConfig);
        
        int hitCountNoDebounce = 0;
        int hitCountDebounce = 0;
        
        noDebounceProcessor->setHitCallback([&](bool hitState, const PostProcessor::HitEvent&) {
            if (hitState) hitCountNoDebounce++;
        });
        
        debounceProcessor->setHitCallback([&](bool hitState, const PostProcessor::HitEvent&) {
            if (hitState) hitCountDebounce++;
        });
        
        std::cout << "Comparing with and without debouncing:" << std::endl;
        std::cout << "Frame | Signal | No Debounce | With Debounce" << std::endl;
        std::cout << "------|--------|-------------|-------------" << std::endl;
        
        for (size_t i = 0; i < rapidHitsSignal.size(); ++i) {
            noDebounceProcessor->processConfidence(rapidHitsSignal[i]);
            debounceProcessor->processConfidence(rapidHitsSignal[i]);
            
            bool noDebounceHit = noDebounceProcessor->hasHit();
            bool debounceHit = debounceProcessor->hasHit();
            
            std::cout << std::setw(5) << i << " | "
                      << std::fixed << std::setprecision(3)
                      << std::setw(6) << rapidHitsSignal[i] << " | "
                      << std::setw(11) << (noDebounceHit ? "HIT" : "---") << " | "
                      << std::setw(13) << (debounceHit ? "HIT" : "---") << std::endl;
        }
        
        std::cout << "\nResults:" << std::endl;
        std::cout << "  Without debouncing: " << hitCountNoDebounce << " hits detected" << std::endl;
        std::cout << "  With debouncing: " << hitCountDebounce << " hits detected" << std::endl;
        std::cout << "  Debounced hits: " << debounceProcessor->getStatistics().debouncedHits.load() << std::endl;
    }
    
    void demoRealtimeProcessing()
    {
        std::cout << "--- Real-time Processing Simulation ---" << std::endl;
        
        auto processor = createPostProcessor(0.6f, 5);
        
        // Simulate real-time confidence updates from AI inference
        std::cout << "Simulating real-time AI inference results (20ms intervals):" << std::endl;
        
        // Generate a realistic confidence pattern
        auto realtimeSignal = generateTestSignal("realtime_demo");
        
        processor->setHitCallback([](bool hitState, const PostProcessor::HitEvent& event) {
            auto now = std::chrono::steady_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>
                (now.time_since_epoch()).count();
            
            if (hitState) {
                std::cout << "[" << timestamp << "ms] *** HIT DETECTED *** "
                          << "Peak: " << std::fixed << std::setprecision(3) << event.peakConfidence << std::endl;
            } else {
                std::cout << "[" << timestamp << "ms] Hit ended (duration: " 
                          << event.hitDurationFrames << " frames)" << std::endl;
            }
        });
        
        std::cout << "Processing " << realtimeSignal.size() << " frames at 20ms intervals..." << std::endl;
        
        for (size_t i = 0; i < realtimeSignal.size(); ++i) {
            float filtered = processor->processConfidence(realtimeSignal[i]);
            
            // Print current status
            std::cout << "Frame " << std::setw(2) << i 
                      << ": Raw=" << std::fixed << std::setprecision(3) << realtimeSignal[i]
                      << ", Filtered=" << std::setprecision(3) << filtered
                      << ", Hit=" << (processor->hasHit() ? "YES" : "NO") << std::endl;
            
            // Simulate 20ms processing interval
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        
        auto stats = processor->getStatistics();
        std::cout << "\nReal-time Processing Summary:" << std::endl;
        std::cout << "  Total frames: " << stats.totalFramesProcessed.load() << std::endl;
        std::cout << "  Hits detected: " << stats.totalHits.load() << std::endl;
        std::cout << "  Average confidence: " << std::fixed << std::setprecision(3) 
                  << stats.averageConfidence.load() << std::endl;
        std::cout << "  Peak confidence: " << std::fixed << std::setprecision(3) 
                  << stats.peakConfidence.load() << std::endl;
    }
    
    void demoPerformanceComparison()
    {
        std::cout << "--- Performance Comparison ---" << std::endl;
        
        const int numSamples = 10000;
        std::vector<float> testData(numSamples);
        
        // Generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        for (int i = 0; i < numSamples; ++i) {
            testData[i] = dist(gen);
        }
        
        // Test different filter sizes
        std::vector<int> filterSizes = {3, 5, 7, 9, 11};
        
        std::cout << "Testing performance with different median filter sizes:" << std::endl;
        std::cout << "Filter Size | Processing Time (μs) | Samples/μs" << std::endl;
        std::cout << "------------|---------------------|----------" << std::endl;
        
        for (int filterSize : filterSizes) {
            auto processor = createPostProcessor(0.6f, filterSize);
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            for (float sample : testData) {
                processor->processConfidence(sample);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            
            double samplesPerMicrosecond = static_cast<double>(numSamples) / duration.count();
            
            std::cout << std::setw(11) << filterSize << " | "
                      << std::setw(19) << duration.count() << " | "
                      << std::fixed << std::setprecision(2) << std::setw(8) << samplesPerMicrosecond << std::endl;
        }
        
        std::cout << "\nNote: Larger filter sizes provide better noise reduction but require more processing." << std::endl;
    }
    
    std::vector<float> generateTestSignal(const std::string& type)
    {
        std::vector<float> signal;
        
        if (type == "moderate_hits") {
            // Signal with a few clear hits and some noise
            signal = {
                0.1f, 0.2f, 0.3f, 0.2f, 0.1f,     // Baseline with noise
                0.7f, 0.8f, 0.9f, 0.8f, 0.7f,     // Clear hit
                0.3f, 0.2f, 0.4f, 0.3f, 0.2f,     // Return to baseline
                0.1f, 0.9f, 0.1f,                 // Single spike (noise)
                0.6f, 0.7f, 0.8f, 0.7f,           // Another hit
                0.2f, 0.1f, 0.1f                  // End
            };
        } else if (type == "realtime_demo") {
            // Realistic AI inference pattern
            signal = {
                0.05f, 0.08f, 0.12f, 0.15f,       // Gradual buildup
                0.25f, 0.35f, 0.45f,              // Rising confidence
                0.65f, 0.75f, 0.85f, 0.92f,       // Hit detected
                0.88f, 0.82f, 0.75f,              // Peak and decline
                0.65f, 0.45f, 0.25f,              // Falling
                0.15f, 0.08f, 0.05f               // Return to baseline
            };
        }
        
        return signal;
    }
};

int main()
{
    try {
        PostProcessorDemo demo;
        demo.runAllDemos();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
} 