#include "PostProcessor.h"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <cmath>

namespace KhDetector {

PostProcessor::PostProcessor(const Config& config) 
    : mConfig(config)
{
    validateConfig();
    
    // Initialize confidence history buffer
    mConfidenceHistory.resize(mConfig.medianFilterSize, 0.0f);
    
    // Initialize statistics
    mStats.isCurrentlyHit.store(false);
    mStats.currentHitDuration.store(0);
    
    std::cout << "PostProcessor initialized:" << std::endl;
    std::cout << "  Median filter size: " << mConfig.medianFilterSize << std::endl;
    std::cout << "  Threshold: " << mConfig.threshold << std::endl;
    std::cout << "  Hysteresis: " << (mConfig.enableHysteresis ? "enabled" : "disabled") << std::endl;
    std::cout << "  Debounce: " << (mConfig.enableDebounce ? "enabled" : "disabled") << std::endl;
}

float PostProcessor::processConfidence(float confidence)
{
    if (!mEnabled.load()) {
        return confidence; // Pass through when disabled
    }
    
    // Clamp confidence to valid range
    confidence = std::clamp(confidence, 0.0f, 1.0f);
    
    // Add to circular buffer
    mConfidenceHistory[mHistoryIndex] = confidence;
    mHistoryIndex = (mHistoryIndex + 1) % mConfig.medianFilterSize;
    
    if (!mHistoryFilled && mHistoryIndex == 0) {
        mHistoryFilled = true;
    }
    
    // Apply median filter
    float smoothedConfidence = applyMedianFilter();
    mLastSmoothedConfidence.store(smoothedConfidence);
    
    // Update hit detection
    updateHitDetection(smoothedConfidence);
    
    // Update statistics
    updateStatistics(confidence, smoothedConfidence);
    
    return smoothedConfidence;
}

PostProcessor::HitEvent PostProcessor::getLastHitEvent() const
{
    return mLastHitEvent;
}

void PostProcessor::reset()
{
    // Reset filter state
    std::fill(mConfidenceHistory.begin(), mConfidenceHistory.end(), 0.0f);
    mHistoryIndex = 0;
    mHistoryFilled = false;
    
    // Reset hit detection state
    bool wasHit = mCurrentHitState;
    mCurrentHitState = false;
    mHitFrameCount = 0;
    mDebounceFrameCount = 0;
    mPeakConfidenceInHit = 0.0f;
    
    // Update atomic state
    mHadHit.store(false);
    mLastSmoothedConfidence.store(0.0f);
    
    // Trigger callback if we were in a hit state
    if (wasHit && mHitCallback) {
        try {
            mHitCallback(false, mLastHitEvent);
        } catch (const std::exception& e) {
            std::cerr << "PostProcessor: Hit callback exception: " << e.what() << std::endl;
        }
    }
    
    // Reset statistics
    mStats.totalFramesProcessed.store(0);
    mStats.totalHits.store(0);
    mStats.falsePositives.store(0);
    mStats.debouncedHits.store(0);
    mStats.averageConfidence.store(0.0);
    mStats.averageSmoothedConfidence.store(0.0);
    mStats.peakConfidence.store(0.0f);
    mStats.currentHitDuration.store(0);
    mStats.isCurrentlyHit.store(false);
    
    // Reset running averages
    mConfidenceSum = 0.0;
    mSmoothedConfidenceSum = 0.0;
    mProcessedCount = 0;
    
    std::cout << "PostProcessor: Reset complete" << std::endl;
}

void PostProcessor::updateConfig(const Config& config)
{
    mConfig = config;
    validateConfig();
    
    // Resize confidence history if needed
    if (mConfidenceHistory.size() != static_cast<size_t>(mConfig.medianFilterSize)) {
        mConfidenceHistory.resize(mConfig.medianFilterSize, 0.0f);
        mHistoryIndex = 0;
        mHistoryFilled = false;
    }
    
    std::cout << "PostProcessor: Configuration updated" << std::endl;
}

void PostProcessor::setHitCallback(HitCallback callback)
{
    mHitCallback = std::move(callback);
}

std::vector<float> PostProcessor::getFilterHistory() const
{
    std::vector<float> history;
    history.reserve(mConfig.medianFilterSize);
    
    if (mHistoryFilled) {
        // Return in chronological order
        for (int i = 0; i < mConfig.medianFilterSize; ++i) {
            int index = (mHistoryIndex + i) % mConfig.medianFilterSize;
            history.push_back(mConfidenceHistory[index]);
        }
    } else {
        // Return only filled portion
        for (int i = 0; i < mHistoryIndex; ++i) {
            history.push_back(mConfidenceHistory[i]);
        }
    }
    
    return history;
}

float PostProcessor::applyMedianFilter()
{
    if (!mHistoryFilled && mHistoryIndex < 3) {
        // Not enough samples for meaningful median, return latest value
        return mConfidenceHistory[(mHistoryIndex - 1 + mConfig.medianFilterSize) % mConfig.medianFilterSize];
    }
    
    // Create a copy for sorting (don't modify original circular buffer)
    std::vector<float> sortBuffer;
    
    if (mHistoryFilled) {
        sortBuffer = mConfidenceHistory;
    } else {
        // Only use filled portion
        sortBuffer.assign(mConfidenceHistory.begin(), mConfidenceHistory.begin() + mHistoryIndex);
    }
    
    // Find median
    size_t n = sortBuffer.size();
    std::nth_element(sortBuffer.begin(), sortBuffer.begin() + n/2, sortBuffer.end());
    
    if (n % 2 == 1) {
        // Odd number of elements
        return sortBuffer[n/2];
    } else {
        // Even number of elements - average of two middle values
        float median1 = sortBuffer[n/2];
        std::nth_element(sortBuffer.begin(), sortBuffer.begin() + n/2 - 1, sortBuffer.end());
        float median2 = sortBuffer[n/2 - 1];
        return (median1 + median2) * 0.5f;
    }
}

void PostProcessor::updateHitDetection(float smoothedConfidence)
{
    bool thresholdMet = applyThreshold(smoothedConfidence);
    
    // Handle debouncing
    if (mConfig.enableDebounce && mDebounceFrameCount > 0) {
        mDebounceFrameCount--;
        if (thresholdMet) {
            mStats.debouncedHits.fetch_add(1);
            return; // Ignore hits during debounce period
        }
    }
    
    if (thresholdMet && !mCurrentHitState) {
        // Start of new hit
        mCurrentHitState = true;
        mHitFrameCount = 1;
        mPeakConfidenceInHit = smoothedConfidence;
        mHitStartTime = std::chrono::steady_clock::now();
        
        // Don't trigger external hit state yet - wait for minimum duration
        
    } else if (thresholdMet && mCurrentHitState) {
        // Continue existing hit
        mHitFrameCount++;
        mPeakConfidenceInHit = std::max(mPeakConfidenceInHit, smoothedConfidence);
        
        // Check if we've reached minimum duration for first time
        if (mHitFrameCount == mConfig.minHitDuration && !mHadHit.load()) {
            triggerHitStateChange(true, smoothedConfidence);
        }
        
        // Check for maximum duration auto-reset
        if (mHitFrameCount >= mConfig.maxHitDuration) {
            triggerHitStateChange(false, smoothedConfidence);
            mCurrentHitState = false;
            mDebounceFrameCount = mConfig.enableDebounce ? mConfig.debounceFrames : 0;
        }
        
    } else if (!thresholdMet && mCurrentHitState) {
        // End of hit
        bool wasValidHit = (mHitFrameCount >= mConfig.minHitDuration);
        
        if (!wasValidHit) {
            mStats.falsePositives.fetch_add(1);
        }
        
        if (mHadHit.load()) {
            triggerHitStateChange(false, smoothedConfidence);
        }
        
        mCurrentHitState = false;
        mDebounceFrameCount = mConfig.enableDebounce ? mConfig.debounceFrames : 0;
    }
    
    // Update statistics
    mStats.currentHitDuration.store(mCurrentHitState ? mHitFrameCount : 0);
    mStats.isCurrentlyHit.store(mCurrentHitState);
}

bool PostProcessor::applyThreshold(float confidence) const
{
    if (mConfig.enableHysteresis) {
        if (mCurrentHitState) {
            // Use lower threshold when already in hit state (hysteresis)
            return confidence >= mConfig.hysteresisLow;
        } else {
            // Use higher threshold when not in hit state
            return confidence >= mConfig.hysteresisHigh;
        }
    } else {
        // Simple threshold
        return confidence >= mConfig.threshold;
    }
}

void PostProcessor::triggerHitStateChange(bool newState, float smoothedConfidence)
{
    mHadHit.store(newState);
    
    // Update hit event information
    if (newState) {
        mLastHitEvent.peakConfidence = mPeakConfidenceInHit;
        mLastHitEvent.smoothedConfidence = smoothedConfidence;
        mLastHitEvent.hitDurationFrames = mHitFrameCount;
        mLastHitEvent.timestamp = mHitStartTime;
        mLastHitEvent.isActive = true;
        
        mStats.totalHits.fetch_add(1);
    } else {
        mLastHitEvent.isActive = false;
        mLastHitEvent.hitDurationFrames = mHitFrameCount;
    }
    
    // Call callback if set
    if (mHitCallback) {
        try {
            mHitCallback(newState, mLastHitEvent);
        } catch (const std::exception& e) {
            std::cerr << "PostProcessor: Hit callback exception: " << e.what() << std::endl;
        }
    }
    
    std::cout << "PostProcessor: Hit state changed to " << (newState ? "HIT" : "NO_HIT") 
              << " (confidence: " << smoothedConfidence << ", duration: " << mHitFrameCount << " frames)" << std::endl;
}

void PostProcessor::updateStatistics(float confidence, float smoothedConfidence)
{
    mProcessedCount++;
    mConfidenceSum += confidence;
    mSmoothedConfidenceSum += smoothedConfidence;
    
    mStats.totalFramesProcessed.fetch_add(1);
    
    // Update running averages
    double avgConfidence = mConfidenceSum / mProcessedCount;
    double avgSmoothedConfidence = mSmoothedConfidenceSum / mProcessedCount;
    
    mStats.averageConfidence.store(avgConfidence);
    mStats.averageSmoothedConfidence.store(avgSmoothedConfidence);
    
    // Update peak confidence
    float currentPeak = mStats.peakConfidence.load();
    if (confidence > currentPeak) {
        mStats.peakConfidence.store(confidence);
    }
}

void PostProcessor::validateConfig()
{
    // Ensure median filter size is odd and at least 3
    if (mConfig.medianFilterSize < 3) {
        mConfig.medianFilterSize = 3;
    }
    if (mConfig.medianFilterSize % 2 == 0) {
        mConfig.medianFilterSize++; // Make it odd
    }
    
    // Clamp threshold values
    mConfig.threshold = std::clamp(mConfig.threshold, 0.0f, 1.0f);
    mConfig.hysteresisLow = std::clamp(mConfig.hysteresisLow, 0.0f, 1.0f);
    mConfig.hysteresisHigh = std::clamp(mConfig.hysteresisHigh, 0.0f, 1.0f);
    
    // Ensure hysteresis values make sense
    if (mConfig.enableHysteresis) {
        if (mConfig.hysteresisLow > mConfig.hysteresisHigh) {
            std::swap(mConfig.hysteresisLow, mConfig.hysteresisHigh);
        }
        // If hysteresis values are not set, derive them from main threshold
        if (mConfig.hysteresisLow == 0.0f && mConfig.hysteresisHigh == 0.0f) {
            mConfig.hysteresisHigh = mConfig.threshold;
            mConfig.hysteresisLow = mConfig.threshold * 0.8f; // 20% hysteresis
        }
    }
    
    // Ensure duration limits are reasonable
    mConfig.minHitDuration = std::max(1, mConfig.minHitDuration);
    mConfig.maxHitDuration = std::max(mConfig.minHitDuration + 1, mConfig.maxHitDuration);
    mConfig.debounceFrames = std::max(0, mConfig.debounceFrames);
}

// Factory functions
std::unique_ptr<PostProcessor> createPostProcessor(float threshold, int medianFilterSize)
{
    PostProcessor::Config config;
    config.threshold = threshold;
    config.medianFilterSize = medianFilterSize;
    return std::make_unique<PostProcessor>(config);
}

PostProcessor::Config createSensitiveConfig()
{
    PostProcessor::Config config;
    config.medianFilterSize = 3;           // Small filter for quick response
    config.threshold = 0.5f;               // Lower threshold
    config.enableHysteresis = true;
    config.hysteresisHigh = 0.5f;
    config.hysteresisLow = 0.4f;           // 20% hysteresis
    config.minHitDuration = 1;             // Accept short hits
    config.maxHitDuration = 50;            // Shorter max duration
    config.enableDebounce = false;         // No debouncing for sensitivity
    config.debounceFrames = 0;
    return config;
}

PostProcessor::Config createRobustConfig()
{
    PostProcessor::Config config;
    config.medianFilterSize = 7;           // Larger filter for stability
    config.threshold = 0.7f;               // Higher threshold
    config.enableHysteresis = true;
    config.hysteresisHigh = 0.7f;
    config.hysteresisLow = 0.6f;           // Strong hysteresis
    config.minHitDuration = 5;             // Require sustained hits
    config.maxHitDuration = 200;           // Allow longer hits
    config.enableDebounce = true;          // Enable debouncing
    config.debounceFrames = 5;             // Strong debouncing
    return config;
}

PostProcessor::Config createFastConfig()
{
    PostProcessor::Config config;
    config.medianFilterSize = 3;           // Minimal filtering
    config.threshold = 0.6f;               // Standard threshold
    config.enableHysteresis = false;       // No hysteresis for speed
    config.minHitDuration = 1;             // Immediate response
    config.maxHitDuration = 100;           // Standard max duration
    config.enableDebounce = false;         // No debouncing
    config.debounceFrames = 0;
    return config;
}

} // namespace KhDetector 