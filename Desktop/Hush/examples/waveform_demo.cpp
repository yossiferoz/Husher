/**
 * @file waveform_demo.cpp
 * @brief Demonstration of scrolling waveform visualization with spectral overlay
 * 
 * This demo shows:
 * - Scrolling waveform display with 50ms SMAA line recycling
 * - Spectral overlay visualization
 * - Colored hit flags
 * - VBO double-buffering for >120 FPS performance
 * - Real-time audio simulation
 */

#include "../src/WaveformData.h"
#include "../src/WaveformRenderer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>
#include <iomanip>

using namespace KhDetector;

class WaveformDemo {
public:
    WaveformDemo() : running_(false), frameCount_(0) {
        // Initialize waveform buffer
        waveformBuffer_ = std::make_shared<WaveformBuffer4K>();
        
        // Configure for high performance
        config_.targetFPS = 120;
        config_.enableVSync = false;
        config_.enableSMAA = true;
        config_.updateIntervalMs = 50.0f;  // 50ms SMAA line recycling
        config_.timeWindowSeconds = 2.0f;   // 2 second display window
        config_.showSpectralOverlay = true;
        config_.showHitFlags = true;
        config_.showGrid = true;
        
        // Initialize renderer (would need OpenGL context in real application)
        renderer_ = std::make_unique<WaveformRenderer>(config_);
        
        // Initialize spectral analyzer
        spectralAnalyzer_ = std::make_unique<SpectralAnalyzer>(320); // 20ms at 16kHz
        spectralAnalyzer_->setSampleRate(16000.0f);
        
        // Initialize random number generator
        gen_.seed(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    
    void start() {
        running_ = true;
        
        // Start audio simulation thread
        audioThread_ = std::thread(&WaveformDemo::audioSimulationLoop, this);
        
        // Start hit detection thread
        hitThread_ = std::thread(&WaveformDemo::hitDetectionLoop, this);
        
        // Start rendering loop (would be called by OpenGL context in real app)
        renderingLoop();
    }
    
    void stop() {
        running_ = false;
        
        if (audioThread_.joinable()) {
            audioThread_.join();
        }
        
        if (hitThread_.joinable()) {
            hitThread_.join();
        }
    }

private:
    std::shared_ptr<WaveformBuffer4K> waveformBuffer_;
    std::unique_ptr<WaveformRenderer> renderer_;
    std::unique_ptr<SpectralAnalyzer> spectralAnalyzer_;
    WaveformConfig config_;
    
    std::atomic<bool> running_;
    std::atomic<bool> hitDetected_{false};
    std::atomic<int> frameCount_;
    
    std::thread audioThread_;
    std::thread hitThread_;
    
    std::mt19937 gen_;
    std::uniform_real_distribution<float> noiseDist_{-0.1f, 0.1f};
    std::uniform_real_distribution<float> hitDist_{0.0f, 1.0f};
    
    void audioSimulationLoop() {
        const float sampleRate = 16000.0f;
        const float frameTime = 1.0f / sampleRate;
        auto lastTime = std::chrono::high_resolution_clock::now();
        
        float phase = 0.0f;
        float fundamentalFreq = 440.0f; // A4
        
        std::cout << "Audio simulation started at " << sampleRate << " Hz" << std::endl;
        
        while (running_) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration<float>(currentTime - lastTime).count();
            
            if (elapsed >= frameTime) {
                // Generate audio sample with multiple components
                float sample = 0.0f;
                
                // Fundamental frequency
                sample += 0.3f * std::sin(2.0f * M_PI * fundamentalFreq * phase);
                
                // Add harmonics for richer spectral content
                sample += 0.15f * std::sin(2.0f * M_PI * fundamentalFreq * 2.0f * phase);
                sample += 0.075f * std::sin(2.0f * M_PI * fundamentalFreq * 3.0f * phase);
                
                // Add some noise
                sample += noiseDist_(gen_);
                
                // Simulate amplitude modulation
                sample *= (0.7f + 0.3f * std::sin(2.0f * M_PI * 2.0f * phase));
                
                // Calculate features for waveform sample
                float rms = std::abs(sample);
                float zcr = calculateZeroCrossingRate(sample);
                float spectralCentroid = fundamentalFreq + 200.0f * std::sin(2.0f * M_PI * 0.5f * phase);
                
                // Check if this should be a hit
                bool isHit = hitDetected_.load();
                
                // Create waveform sample
                WaveformSample waveformSample(sample, rms, spectralCentroid, zcr, isHit);
                waveformBuffer_->push(waveformSample);
                
                // Update phase
                phase += frameTime;
                if (phase >= 1.0f) {
                    phase -= 1.0f;
                    
                    // Occasionally change fundamental frequency
                    if (hitDist_(gen_) < 0.1f) {
                        fundamentalFreq = 220.0f + 440.0f * hitDist_(gen_);
                    }
                }
                
                lastTime = currentTime;
            }
            
            // Small sleep to prevent 100% CPU usage
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        
        std::cout << "Audio simulation stopped" << std::endl;
    }
    
    void hitDetectionLoop() {
        std::cout << "Hit detection started" << std::endl;
        
        while (running_) {
            // Simulate random hit detection
            if (hitDist_(gen_) < 0.02f) { // 2% chance per check
                hitDetected_ = true;
                std::cout << "Hit detected!" << std::endl;
                
                // Keep hit flag active for a short time
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                hitDetected_ = false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::cout << "Hit detection stopped" << std::endl;
    }
    
    void renderingLoop() {
        std::cout << "Rendering loop started (simulated - no actual OpenGL)" << std::endl;
        std::cout << "Target FPS: " << config_.targetFPS << std::endl;
        std::cout << "SMAA recycling interval: " << config_.updateIntervalMs << "ms" << std::endl;
        
        const auto targetFrameTime = std::chrono::microseconds(
            static_cast<int64_t>(1000000.0f / config_.targetFPS));
        
        auto lastFrame = std::chrono::high_resolution_clock::now();
        auto lastFPSUpdate = lastFrame;
        int framesSinceUpdate = 0;
        
        while (running_) {
            auto frameStart = std::chrono::high_resolution_clock::now();
            
            // Get waveform samples for rendering
            auto samples = waveformBuffer_->getSamplesInTimeWindow(config_.timeWindowSeconds);
            
            // Generate spectral frames if we have enough samples
            std::vector<SpectralFrame> spectralFrames;
            if (samples.size() >= 320) {
                for (size_t i = 0; i + 320 <= samples.size(); i += 160) { // 50% overlap
                    std::vector<float> frameData(320);
                    for (size_t j = 0; j < 320; ++j) {
                        frameData[j] = samples[i + j].amplitude;
                    }
                    spectralFrames.push_back(spectralAnalyzer_->analyze(frameData.data(), 320));
                }
            }
            
            // Simulate rendering (in real app, this would call renderer_->render())
            simulateRendering(samples, spectralFrames);
            
            frameCount_++;
            framesSinceUpdate++;
            
            // Update FPS counter every second
            auto now = std::chrono::high_resolution_clock::now();
            auto fpsDuration = std::chrono::duration<float>(now - lastFPSUpdate).count();
            if (fpsDuration >= 1.0f) {
                float currentFPS = framesSinceUpdate / fpsDuration;
                
                std::cout << "FPS: " << std::fixed << std::setprecision(1) << currentFPS
                         << " | Samples: " << samples.size()
                         << " | Spectral frames: " << spectralFrames.size()
                         << " | Hit: " << (hitDetected_.load() ? "YES" : "NO")
                         << std::endl;
                
                framesSinceUpdate = 0;
                lastFPSUpdate = now;
            }
            
            // Frame rate limiting
            auto frameEnd = std::chrono::high_resolution_clock::now();
            auto frameTime = frameEnd - frameStart;
            
            if (frameTime < targetFrameTime) {
                std::this_thread::sleep_for(targetFrameTime - frameTime);
            }
        }
        
        std::cout << "Rendering loop stopped" << std::endl;
        std::cout << "Total frames rendered: " << frameCount_.load() << std::endl;
    }
    
    void simulateRendering(const std::vector<WaveformSample>& samples,
                          const std::vector<SpectralFrame>& spectralFrames) {
        // In a real application, this would call:
        // renderer_->render(samples, spectralFrames);
        
        // For demo purposes, we just simulate the work
        if (!samples.empty()) {
            // Simulate VBO updates and OpenGL draw calls
            volatile float sum = 0.0f;
            for (const auto& sample : samples) {
                sum += sample.amplitude * sample.amplitude;
            }
            
            // Simulate spectral overlay rendering
            for (const auto& frame : spectralFrames) {
                for (int i = 0; i < SpectralFrame::kNumBins; ++i) {
                    sum += frame.magnitudes[i];
                }
            }
            
            // Prevent compiler optimization
            (void)sum;
        }
    }
    
    float calculateZeroCrossingRate(float currentSample) {
        // Simplified ZCR calculation (would need sample history in real implementation)
        static float lastSample = 0.0f;
        float zcr = 0.0f;
        
        if ((lastSample >= 0 && currentSample < 0) || (lastSample < 0 && currentSample >= 0)) {
            zcr = 1.0f;
        }
        
        lastSample = currentSample;
        return zcr;
    }
};

int main() {
    std::cout << "=== KhDetector Waveform Visualization Demo ===" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Scrolling waveform with 50ms SMAA line recycling" << std::endl;
    std::cout << "- Spectral overlay visualization" << std::endl;
    std::cout << "- Colored hit flags" << std::endl;
    std::cout << "- VBO double-buffering for >120 FPS" << std::endl;
    std::cout << "- Real-time audio simulation at 16kHz" << std::endl;
    std::cout << std::endl;
    
    try {
        WaveformDemo demo;
        
        std::cout << "Starting demo... Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // Run for 30 seconds or until interrupted
        std::thread demoThread([&demo]() {
            demo.start();
        });
        
        // Wait for demo to complete or user interruption
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        std::cout << std::endl;
        std::cout << "Stopping demo..." << std::endl;
        demo.stop();
        
        if (demoThread.joinable()) {
            demoThread.join();
        }
        
        std::cout << "Demo completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 