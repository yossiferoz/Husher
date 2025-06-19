/**
 * @file decimation_demo.cpp
 * @brief Demonstration of the complete audio decimation pipeline
 * 
 * This file shows how the KhDetectorProcessor uses the PolyphaseDecimator
 * and RingBuffer to process stereo audio input into 20ms mono frames.
 */

#include "../src/PolyphaseDecimator.h"
#include "../src/RingBuffer.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <chrono>

using namespace KhDetector;

// Audio processing constants (matching KhDetectorProcessor)
constexpr int kTargetSampleRate = 16000;
constexpr int kFrameSizeMs = 20;
constexpr int kFrameSize = (kTargetSampleRate * kFrameSizeMs) / 1000; // 320 samples
constexpr size_t kRingBufferSize = 2048;

#ifndef DECIM_FACTOR
#define DECIM_FACTOR 3  // Default: 48kHz -> 16kHz
#endif

void generateTestAudio(std::vector<float>& left, std::vector<float>& right, 
                      double sampleRate, double durationSeconds)
{
    int numSamples = static_cast<int>(sampleRate * durationSeconds);
    left.resize(numSamples);
    right.resize(numSamples);
    
    // Generate test signals: 1kHz sine wave with some harmonics
    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        
        // Left channel: fundamental + third harmonic
        left[i] = 0.3f * std::sin(2.0 * M_PI * 1000.0 * t) + 
                  0.1f * std::sin(2.0 * M_PI * 3000.0 * t);
        
        // Right channel: fundamental + fifth harmonic
        right[i] = 0.3f * std::sin(2.0 * M_PI * 1000.0 * t) + 
                   0.05f * std::sin(2.0 * M_PI * 5000.0 * t);
    }
}

void processFrame(const float* frameData, int frameSize, int frameIndex)
{
    // Demonstrate frame processing (similar to KhDetectorProcessor::processDecimatedFrame)
    
    // Calculate RMS energy
    float energy = 0.0f;
    for (int i = 0; i < frameSize; ++i)
    {
        energy += frameData[i] * frameData[i];
    }
    energy = std::sqrt(energy / frameSize);
    
    // Calculate peak amplitude
    float peak = 0.0f;
    for (int i = 0; i < frameSize; ++i)
    {
        peak = std::max(peak, std::abs(frameData[i]));
    }
    
    // Calculate zero crossing rate (simple spectral measure)
    int zeroCrossings = 0;
    for (int i = 1; i < frameSize; ++i)
    {
        if ((frameData[i-1] >= 0 && frameData[i] < 0) ||
            (frameData[i-1] < 0 && frameData[i] >= 0))
        {
            zeroCrossings++;
        }
    }
    float zcr = static_cast<float>(zeroCrossings) / frameSize;
    
    // Display frame analysis
    std::cout << "Frame " << std::setw(3) << frameIndex 
              << ": RMS=" << std::fixed << std::setprecision(4) << energy
              << ", Peak=" << std::setprecision(4) << peak
              << ", ZCR=" << std::setprecision(3) << zcr << std::endl;
}

void demonstrateDecimationPipeline()
{
    std::cout << "=== Audio Decimation Pipeline Demo ===" << std::endl;
    std::cout << "Decimation Factor: " << DECIM_FACTOR << std::endl;
    std::cout << "Target Sample Rate: " << kTargetSampleRate << " Hz" << std::endl;
    std::cout << "Frame Size: " << kFrameSize << " samples (" << kFrameSizeMs << "ms)" << std::endl;
    std::cout << std::endl;
    
    // Generate test audio (500ms at 48kHz)
    const double inputSampleRate = 48000.0;
    const double duration = 0.5; // seconds
    
    std::vector<float> leftChannel, rightChannel;
    generateTestAudio(leftChannel, rightChannel, inputSampleRate, duration);
    
    std::cout << "Generated " << leftChannel.size() << " samples at " 
              << inputSampleRate << " Hz" << std::endl;
    
    // Initialize processing components
    PolyphaseDecimator<DECIM_FACTOR> decimator;
    RingBuffer<float, kRingBufferSize> decimatedBuffer;
    
    // Working buffers
    std::vector<float> decimatedSamples(leftChannel.size() / DECIM_FACTOR + 1);
    std::vector<float> processingFrame(kFrameSize);
    
    // Process audio in blocks (simulating real-time processing)
    const int blockSize = 480; // 10ms blocks at 48kHz
    int totalProcessedFrames = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t offset = 0; offset < leftChannel.size(); offset += blockSize)
    {
        int samplesThisBlock = std::min(blockSize, 
                                       static_cast<int>(leftChannel.size() - offset));
        
        // Decimate stereo to mono
        int decimatedCount = decimator.processStereoToMono(
            leftChannel.data() + offset,
            rightChannel.data() + offset,
            decimatedSamples.data(),
            samplesThisBlock
        );
        
        // Enqueue decimated samples
        for (int i = 0; i < decimatedCount; ++i)
        {
            if (!decimatedBuffer.push(decimatedSamples[i]))
            {
                std::cout << "Warning: Ring buffer overflow!" << std::endl;
            }
        }
        
        // Process complete frames
        while (decimatedBuffer.size() >= kFrameSize)
        {
            int extractedSamples = decimatedBuffer.pop_bulk(
                processingFrame.data(), kFrameSize
            );
            
            if (extractedSamples == kFrameSize)
            {
                processFrame(processingFrame.data(), kFrameSize, totalProcessedFrames);
                totalProcessedFrames++;
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    std::cout << std::endl;
    std::cout << "Processing Summary:" << std::endl;
    std::cout << "- Input samples: " << leftChannel.size() << std::endl;
    std::cout << "- Processed frames: " << totalProcessedFrames << std::endl;
    std::cout << "- Expected frames: " << static_cast<int>(duration * 1000 / kFrameSizeMs) << std::endl;
    std::cout << "- Remaining in buffer: " << decimatedBuffer.size() << " samples" << std::endl;
    std::cout << "- Processing time: " << processingTime.count() << " μs" << std::endl;
    
    // Calculate real-time performance factor
    double realTimeUs = duration * 1000000; // Real-time duration in microseconds
    double rtFactor = realTimeUs / processingTime.count();
    std::cout << "- Real-time factor: " << std::fixed << std::setprecision(1) 
              << rtFactor << "x" << std::endl;
}

void demonstrateSIMDPerformance()
{
    std::cout << std::endl << "=== SIMD Performance Test ===" << std::endl;
    
    // Generate longer test signal for performance measurement
    const double sampleRate = 48000.0;
    const double duration = 10.0; // 10 seconds
    
    std::vector<float> leftChannel, rightChannel;
    generateTestAudio(leftChannel, rightChannel, sampleRate, duration);
    
    PolyphaseDecimator<DECIM_FACTOR> decimator;
    std::vector<float> output(leftChannel.size() / DECIM_FACTOR + 1);
    
    // Warm-up run
    decimator.processStereoToMono(leftChannel.data(), rightChannel.data(),
                                 output.data(), 1000);
    decimator.reset();
    
    // Performance measurement
    auto startTime = std::chrono::high_resolution_clock::now();
    
    int totalOutput = decimator.processStereoToMono(
        leftChannel.data(), rightChannel.data(),
        output.data(), leftChannel.size()
    );
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    std::cout << "SIMD Performance Results:" << std::endl;
    std::cout << "- Input samples: " << leftChannel.size() << std::endl;
    std::cout << "- Output samples: " << totalOutput << std::endl;
    std::cout << "- Processing time: " << processingTime.count() << " μs" << std::endl;
    
    // Calculate samples per second
    double samplesPerSecond = static_cast<double>(leftChannel.size()) / 
                             (processingTime.count() / 1000000.0);
    std::cout << "- Throughput: " << std::scientific << std::setprecision(2) 
              << samplesPerSecond << " samples/sec" << std::endl;
    
    // Real-time performance
    double realTimeUs = duration * 1000000;
    double rtFactor = realTimeUs / processingTime.count();
    std::cout << "- Real-time factor: " << std::fixed << std::setprecision(1) 
              << rtFactor << "x" << std::endl;
    
    // SIMD architecture detection
    std::cout << "- SIMD Support: ";
#if defined(KHDETECTOR_USE_AVX)
    std::cout << "AVX";
#elif defined(KHDETECTOR_USE_SSE2)
    std::cout << "SSE2/SSE4.1";
#elif defined(KHDETECTOR_USE_NEON)
    std::cout << "ARM NEON";
#else
    std::cout << "Scalar (no SIMD)";
#endif
    std::cout << std::endl;
}

int main()
{
    std::cout << "KhDetector Audio Processing Pipeline Demonstration" << std::endl;
    std::cout << "=================================================" << std::endl << std::endl;
    
    try {
        demonstrateDecimationPipeline();
        demonstrateSIMDPerformance();
        
        std::cout << std::endl << "Demo completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 