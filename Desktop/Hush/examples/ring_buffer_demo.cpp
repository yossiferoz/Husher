/**
 * @file ring_buffer_demo.cpp
 * @brief Simple demonstration of the RingBuffer usage in audio applications
 * 
 * This file demonstrates how to use the lock-free RingBuffer template
 * in real-world audio plugin scenarios.
 */

#include "../src/RingBuffer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

using namespace KhDetector;

// Demo 1: Basic usage
void basicUsageDemo()
{
    std::cout << "=== Basic Usage Demo ===" << std::endl;
    
    RingBuffer<float, 16> buffer;
    
    // Push some audio samples
    std::vector<float> audioSamples = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    
    std::cout << "Pushing " << audioSamples.size() << " samples..." << std::endl;
    for (float sample : audioSamples) {
        if (buffer.push(sample)) {
            std::cout << "  Pushed: " << sample << std::endl;
        } else {
            std::cout << "  Buffer full! Cannot push: " << sample << std::endl;
        }
    }
    
    std::cout << "Buffer size: " << buffer.size() << " / " << buffer.capacity() << std::endl;
    
    // Pop and process samples
    std::cout << "Popping samples..." << std::endl;
    float sample;
    while (buffer.pop(sample)) {
        std::cout << "  Popped: " << sample << " (processed: " << sample * 2.0f << ")" << std::endl;
    }
    
    std::cout << "Buffer is now empty: " << (buffer.empty() ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// Demo 2: Bulk operations
void bulkOperationsDemo()
{
    std::cout << "=== Bulk Operations Demo ===" << std::endl;
    
    RingBuffer<int, 32> buffer;
    
    // Prepare test data
    std::vector<int> inputData(20);
    std::iota(inputData.begin(), inputData.end(), 1); // 1, 2, 3, ..., 20
    
    // Bulk push
    size_t pushed = buffer.push_bulk(inputData.data(), inputData.size());
    std::cout << "Bulk pushed " << pushed << " items out of " << inputData.size() << std::endl;
    
    // Bulk pop
    std::vector<int> outputData(25); // Try to pop more than available
    size_t popped = buffer.pop_bulk(outputData.data(), outputData.size());
    std::cout << "Bulk popped " << popped << " items" << std::endl;
    
    // Verify data integrity
    bool dataIntegrityOk = true;
    for (size_t i = 0; i < popped; ++i) {
        if (outputData[i] != inputData[i]) {
            dataIntegrityOk = false;
            break;
        }
    }
    
    std::cout << "Data integrity: " << (dataIntegrityOk ? "OK" : "FAILED") << std::endl;
    std::cout << std::endl;
}

// Demo 3: Producer-Consumer simulation
void producerConsumerDemo()
{
    std::cout << "=== Producer-Consumer Demo ===" << std::endl;
    
    RingBuffer<double, 256> buffer;
    const int numSamples = 1000;
    
    std::atomic<bool> producerDone{false};
    std::atomic<int> samplesProduced{0};
    std::atomic<int> samplesConsumed{0};
    
    // Producer thread (simulates audio callback)
    std::thread producer([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(-1.0, 1.0);
        
        for (int i = 0; i < numSamples; ++i) {
            double sample = dis(gen);
            
            // Simulate audio processing timing
            while (!buffer.push(sample)) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            
            samplesProduced++;
            
            // Simulate sample rate timing (e.g., 48kHz = ~20.8μs between samples)
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
        
        producerDone = true;
    });
    
    // Consumer thread (simulates GUI or analysis thread)
    std::thread consumer([&]() {
        double sample;
        
        while (!producerDone || !buffer.empty()) {
            if (buffer.pop(sample)) {
                samplesConsumed++;
                // Simulate processing
                double processed = sample * 0.8; // Simple gain
                (void)processed; // Suppress unused variable warning
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });
    
    // Monitor progress
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!producerDone || !buffer.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        
        std::cout << "\rProduced: " << samplesProduced.load() 
                  << ", Consumed: " << samplesConsumed.load()
                  << ", Buffer: " << buffer.size() << "    " << std::flush;
    }
    
    producer.join();
    consumer.join();
    
    std::cout << std::endl;
    std::cout << "Final - Produced: " << samplesProduced.load() 
              << ", Consumed: " << samplesConsumed.load() << std::endl;
    std::cout << "Success: " << (samplesProduced == samplesConsumed && samplesProduced == numSamples ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// Demo 4: Different data types
void dataTypesDemo()
{
    std::cout << "=== Data Types Demo ===" << std::endl;
    
    // Audio sample struct
    struct AudioSample {
        float left;
        float right;
        uint32_t timestamp;
        
        bool operator==(const AudioSample& other) const {
            return left == other.left && right == other.right && timestamp == other.timestamp;
        }
    };
    
    RingBuffer<AudioSample, 8> stereoBuffer;
    
    // Push stereo samples
    AudioSample samples[] = {
        {-0.5f, 0.3f, 1000},
        {0.2f, -0.1f, 1001},
        {0.8f, 0.9f, 1002}
    };
    
    for (const auto& sample : samples) {
        if (stereoBuffer.push(sample)) {
            std::cout << "Pushed stereo sample: L=" << sample.left 
                      << ", R=" << sample.right 
                      << ", T=" << sample.timestamp << std::endl;
        }
    }
    
    // Pop and verify
    AudioSample retrieved;
    int index = 0;
    while (stereoBuffer.pop(retrieved)) {
        bool matches = (retrieved == samples[index]);
        std::cout << "Retrieved sample " << index << ": " 
                  << (matches ? "MATCH" : "MISMATCH") << std::endl;
        index++;
    }
    
    std::cout << std::endl;
}

int main()
{
    std::cout << "RingBuffer Demonstration" << std::endl;
    std::cout << "========================" << std::endl << std::endl;
    
    try {
        basicUsageDemo();
        bulkOperationsDemo();
        producerConsumerDemo();
        dataTypesDemo();
        
        std::cout << "All demos completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 