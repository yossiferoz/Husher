#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace KhDetector {

/**
 * @brief Data structure for a single waveform sample with timing and spectral info
 */
struct WaveformSample {
    float amplitude = 0.0f;          // Raw amplitude (-1.0 to 1.0)
    float rms = 0.0f;                // RMS energy
    float spectralCentroid = 0.0f;   // Spectral centroid (brightness)
    float zeroCrossingRate = 0.0f;   // Zero crossing rate
    std::chrono::high_resolution_clock::time_point timestamp;
    bool isHit = false;              // Hit detection flag
    
    WaveformSample() : timestamp(std::chrono::high_resolution_clock::now()) {}
    
    WaveformSample(float amp, float r, float sc, float zcr, bool hit = false)
        : amplitude(amp), rms(r), spectralCentroid(sc), zeroCrossingRate(zcr)
        , timestamp(std::chrono::high_resolution_clock::now()), isHit(hit) {}
};

/**
 * @brief Spectral data for frequency domain visualization
 */
struct SpectralFrame {
    static constexpr int kNumBins = 64;  // Number of frequency bins for display
    std::array<float, kNumBins> magnitudes;
    float fundamentalFreq = 0.0f;
    float spectralCentroid = 0.0f;
    float spectralRolloff = 0.0f;
    std::chrono::high_resolution_clock::time_point timestamp;
    
    SpectralFrame() : timestamp(std::chrono::high_resolution_clock::now()) {
        magnitudes.fill(0.0f);
    }
};

/**
 * @brief Configuration for waveform display
 */
struct WaveformConfig {
    // Display parameters
    float timeWindowSeconds = 2.0f;        // Time window to display
    float updateIntervalMs = 50.0f;        // Update interval (20 FPS for SMAA lines)
    int maxSamplesPerLine = 1024;          // Max samples per scrolling line
    
    // Visual parameters
    struct Colors {
        float waveform[4] = {0.2f, 0.8f, 0.2f, 1.0f};      // Green waveform
        float spectral[4] = {0.8f, 0.4f, 0.2f, 0.7f};      // Orange spectral overlay
        float hitFlag[4] = {1.0f, 0.2f, 0.2f, 1.0f};       // Red hit flags
        float background[4] = {0.05f, 0.05f, 0.05f, 1.0f}; // Dark background
        float grid[4] = {0.3f, 0.3f, 0.3f, 0.3f};          // Grid lines
    } colors;
    
    // Performance settings
    bool enableVSync = false;              // Disable VSync for >120 FPS
    int targetFPS = 120;                   // Target frame rate
    bool enableSMAA = true;                // Enable SMAA anti-aliasing
    int doubleBufferCount = 2;             // Number of VBOs for double buffering
    
    // Audio analysis
    float spectralThreshold = 0.01f;       // Minimum spectral magnitude to display
    bool showSpectralOverlay = true;       // Show spectral data overlay
    bool showHitFlags = true;              // Show hit detection flags
    bool showGrid = true;                  // Show time/amplitude grid
};

/**
 * @brief Thread-safe circular buffer for waveform data
 */
template<size_t Capacity>
class WaveformBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    WaveformBuffer() : writeIndex_(0), readIndex_(0) {}
    
    /**
     * @brief Add new waveform sample (producer thread)
     */
    bool push(const WaveformSample& sample) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t nextWrite = (writeIndex_ + 1) & (Capacity - 1);
        if (nextWrite == readIndex_) {
            // Buffer full, overwrite oldest
            readIndex_ = (readIndex_ + 1) & (Capacity - 1);
        }
        
        buffer_[writeIndex_] = sample;
        writeIndex_ = nextWrite;
        return true;
    }
    
    /**
     * @brief Get samples for rendering (consumer thread)
     */
    std::vector<WaveformSample> getSamples(size_t maxSamples = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<WaveformSample> result;
        if (writeIndex_ == readIndex_) {
            return result; // Empty buffer
        }
        
        size_t availableSamples = (writeIndex_ - readIndex_) & (Capacity - 1);
        if (writeIndex_ < readIndex_) {
            availableSamples = Capacity - readIndex_ + writeIndex_;
        }
        
        if (maxSamples > 0) {
            availableSamples = std::min(availableSamples, maxSamples);
        }
        
        result.reserve(availableSamples);
        
        size_t idx = readIndex_;
        for (size_t i = 0; i < availableSamples; ++i) {
            result.push_back(buffer_[idx]);
            idx = (idx + 1) & (Capacity - 1);
        }
        
        return result;
    }
    
    /**
     * @brief Get recent samples within time window
     */
    std::vector<WaveformSample> getSamplesInTimeWindow(float windowSeconds) const {
        auto now = std::chrono::high_resolution_clock::now();
        auto cutoff = now - std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<float>(windowSeconds));
        
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<WaveformSample> result;
        
        // Search backwards from most recent
        size_t idx = (writeIndex_ - 1) & (Capacity - 1);
        while (idx != readIndex_) {
            const auto& sample = buffer_[idx];
            if (sample.timestamp < cutoff) {
                break; // Too old
            }
            result.push_back(sample);
            idx = (idx - 1) & (Capacity - 1);
        }
        
        // Reverse to get chronological order
        std::reverse(result.begin(), result.end());
        return result;
    }
    
    /**
     * @brief Clear all samples
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        readIndex_ = writeIndex_ = 0;
    }
    
    /**
     * @brief Get current size
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return (writeIndex_ - readIndex_) & (Capacity - 1);
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return writeIndex_ == readIndex_;
    }
    
private:
    mutable std::mutex mutex_;
    std::array<WaveformSample, Capacity> buffer_;
    std::atomic<size_t> writeIndex_;
    std::atomic<size_t> readIndex_;
};

/**
 * @brief Spectral analyzer for frequency domain data
 */
class SpectralAnalyzer {
public:
    SpectralAnalyzer(int frameSize = 512);
    ~SpectralAnalyzer();
    
    /**
     * @brief Analyze audio frame and extract spectral features
     */
    SpectralFrame analyze(const float* audioData, int numSamples);
    
    /**
     * @brief Set analysis parameters
     */
    void setFrameSize(int frameSize);
    void setSampleRate(float sampleRate);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Type aliases for common buffer sizes
using WaveformBuffer2K = WaveformBuffer<2048>;
using WaveformBuffer4K = WaveformBuffer<4096>;
using WaveformBuffer8K = WaveformBuffer<8192>;

} // namespace KhDetector 