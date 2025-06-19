#pragma once

#include <vector>
#include <atomic>
#include <functional>
#include <memory>

namespace KhDetector {

/**
 * PostProcessor - Handles post-processing of AI confidence values
 * 
 * Features:
 * - Median filter smoothing for noise reduction
 * - Configurable threshold detection with optional hysteresis
 * - Minimum/maximum hit duration filtering
 * - Debouncing to prevent rapid successive hits
 * - Thread-safe atomic operations for real-time use
 * - Comprehensive statistics and event callbacks
 */
class PostProcessor 
{
public:
    /**
     * Configuration structure for PostProcessor
     */
    struct Config 
    {
        int medianFilterSize = 5;           // Size of median filter (odd numbers preferred)
        float threshold = 0.6f;             // Hit detection threshold (0.0-1.0)
        float hysteresisLower = 0.55f;      // Lower threshold for hysteresis
        bool enableHysteresis = false;      // Enable hysteresis thresholding
        int minHitDuration = 3;             // Minimum hit duration in frames
        int maxHitDuration = 100;           // Maximum hit duration before auto-reset
        bool enableDebounce = true;         // Enable debouncing
        int debounceFrames = 2;             // Debounce period in frames
    };

    /**
     * Hit event information
     */
    struct HitEvent 
    {
        bool isHit = false;
        float confidence = 0.0f;
        float peakConfidence = 0.0f;
        int duration = 0;
        uint64_t timestamp = 0;
    };

    /**
     * Statistics structure
     */
    struct Statistics 
    {
        std::atomic<uint64_t> totalFramesProcessed{0};
        std::atomic<uint64_t> totalHits{0};
        std::atomic<uint64_t> falsePositives{0};
        std::atomic<uint64_t> debouncedHits{0};
        std::atomic<double> averageConfidence{0.0};
        std::atomic<double> averageSmoothedConfidence{0.0};
        std::atomic<float> peakConfidence{0.0f};
        std::atomic<int> currentHitDuration{0};
        std::atomic<bool> isCurrentlyHit{false};
    };

    /**
     * Hit callback function type
     */
    using HitCallback = std::function<void(bool hitState, const HitEvent& event)>;

    /**
     * Constructor
     */
    explicit PostProcessor(const Config& config = Config{});

    /**
     * Process a confidence value and return smoothed result
     * Thread-safe for single producer/single consumer
     */
    float processConfidence(float confidence);

    /**
     * Check if currently in hit state (thread-safe)
     */
    bool hasHit() const { return mHadHit.load(); }

    /**
     * Get current smoothed confidence value (thread-safe)
     */
    float getCurrentSmoothedConfidence() const { return mLastSmoothedConfidence.load(); }

    /**
     * Get last hit event information
     */
    HitEvent getLastHitEvent() const;

    /**
     * Reset all state and statistics
     */
    void reset();

    /**
     * Update configuration (will reset filter state)
     */
    void updateConfig(const Config& config);

    /**
     * Get current configuration
     */
    const Config& getConfig() const { return mConfig; }

    /**
     * Set hit state change callback
     */
    void setHitCallback(HitCallback callback);

    /**
     * Enable/disable processing
     */
    void setEnabled(bool enabled) { mEnabled.store(enabled); }
    bool isEnabled() const { return mEnabled.load(); }

    /**
     * Get processing statistics (thread-safe)
     */
    Statistics getStatistics() const { return mStats; }

    /**
     * Get filter history for debugging/visualization
     */
    std::vector<float> getFilterHistory() const;

private:
    // Configuration
    Config mConfig;
    
    // Filter state
    std::vector<float> mConfidenceHistory;
    int mHistoryIndex = 0;
    bool mHistoryFilled = false;
    
    // Hit detection state
    bool mCurrentHitState = false;
    int mHitFrameCount = 0;
    int mDebounceFrameCount = 0;
    float mPeakConfidenceInHit = 0.0f;
    HitEvent mLastHitEvent;
    
    // Thread-safe state
    std::atomic<bool> mHadHit{false};
    std::atomic<bool> mEnabled{true};
    std::atomic<float> mLastSmoothedConfidence{0.0f};
    
    // Statistics
    Statistics mStats;
    double mConfidenceSum = 0.0;
    double mSmoothedConfidenceSum = 0.0;
    uint64_t mProcessedCount = 0;
    
    // Callback
    HitCallback mHitCallback;
    
    // Private methods
    float applyMedianFilter();
    bool applyThreshold(float confidence) const;
    void updateHitDetection(float smoothedConfidence);
    void triggerHitStateChange(bool newState, float smoothedConfidence);
    void updateStatistics(float confidence, float smoothedConfidence);
    void validateConfig();
};

// Factory functions for common configurations
std::unique_ptr<PostProcessor> createPostProcessor(float threshold = 0.6f, int medianFilterSize = 5);
PostProcessor::Config createSensitiveConfig();
PostProcessor::Config createRobustConfig();
PostProcessor::Config createFastConfig();

} // namespace KhDetector 