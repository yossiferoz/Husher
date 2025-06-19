#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <complex>
#include <chrono>
#include <atomic>
#include <memory>
#include <thread>
#include "../src/PolyphaseDecimator.h"

using namespace KhDetector;

class PolyphaseDecimatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test signals
        generateTestSignals();
    }

    void TearDown() override
    {
        // Clean up
    }

    void generateTestSignals()
    {
        const int sampleRate = 48000;
        const int numSamples = 4800; // 100ms at 48kHz
        
        testSignal.resize(numSamples);
        stereoLeft.resize(numSamples);
        stereoRight.resize(numSamples);
        
        // Generate test sine wave at 1kHz
        for (int i = 0; i < numSamples; ++i)
        {
            float t = static_cast<float>(i) / sampleRate;
            testSignal[i] = 0.5f * std::sin(2.0f * M_PI * 1000.0f * t);
            
            // Create stereo signals with phase difference
            stereoLeft[i] = 0.3f * std::sin(2.0f * M_PI * 1000.0f * t);
            stereoRight[i] = 0.3f * std::sin(2.0f * M_PI * 1000.0f * t + M_PI / 4);
        }
    }

    // Helper function to calculate SNR in dB
    double calculateSNR(const std::vector<float>& signal, const std::vector<float>& noise)
    {
        double signalPower = 0.0;
        double noisePower = 0.0;
        
        size_t minSize = std::min(signal.size(), noise.size());
        
        for (size_t i = 0; i < minSize; ++i)
        {
            signalPower += signal[i] * signal[i];
            noisePower += noise[i] * noise[i];
        }
        
        signalPower /= minSize;
        noisePower /= minSize;
        
        if (noisePower == 0.0) return std::numeric_limits<double>::infinity();
        
        return 10.0 * std::log10(signalPower / noisePower);
    }

    // Helper function to generate ideal decimated reference signal
    std::vector<float> generateIdealDecimatedSine(double frequency, double sampleRate, 
                                                 double amplitude, int numSamples, int decimationFactor)
    {
        std::vector<float> decimated;
        decimated.reserve(numSamples / decimationFactor);
        
        double outputSampleRate = sampleRate / decimationFactor;
        
        for (int i = 0; i < numSamples / decimationFactor; ++i)
        {
            double t = static_cast<double>(i) / outputSampleRate;
            decimated.push_back(amplitude * std::sin(2.0 * M_PI * frequency * t));
        }
        
        return decimated;
    }

    // Helper function to check for dynamic memory allocation
    class MemoryTracker 
    {
    public:
        static std::atomic<size_t> allocationCount;
        static std::atomic<size_t> deallocationCount;
        static std::atomic<bool> trackingEnabled;
        
        static void reset() 
        {
            allocationCount.store(0);
            deallocationCount.store(0);
            trackingEnabled.store(true);
        }
        
        static void disable() 
        {
            trackingEnabled.store(false);
        }
        
        static size_t getAllocations() 
        {
            return allocationCount.load();
        }
        
        static size_t getDeallocations() 
        {
            return deallocationCount.load();
        }
    };

    std::vector<float> testSignal;
    std::vector<float> stereoLeft;
    std::vector<float> stereoRight;
};

// Static member definitions
std::atomic<size_t> PolyphaseDecimatorTest::MemoryTracker::allocationCount{0};
std::atomic<size_t> PolyphaseDecimatorTest::MemoryTracker::deallocationCount{0};
std::atomic<bool> PolyphaseDecimatorTest::MemoryTracker::trackingEnabled{false};

// Custom new/delete operators for memory tracking (only when tracking is enabled)
void* operator new(size_t size) 
{
    void* ptr = std::malloc(size);
    if (PolyphaseDecimatorTest::MemoryTracker::trackingEnabled.load()) 
    {
        PolyphaseDecimatorTest::MemoryTracker::allocationCount.fetch_add(1);
    }
    return ptr;
}

void operator delete(void* ptr) noexcept 
{
    if (PolyphaseDecimatorTest::MemoryTracker::trackingEnabled.load()) 
    {
        PolyphaseDecimatorTest::MemoryTracker::deallocationCount.fetch_add(1);
    }
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept 
{
    if (PolyphaseDecimatorTest::MemoryTracker::trackingEnabled.load()) 
    {
        PolyphaseDecimatorTest::MemoryTracker::deallocationCount.fetch_add(1);
    }
    std::free(ptr);
}

// ============================================================================
// SNR TESTS - Expect ≥ 70 dB SNR at 1 kHz sine wave
// ============================================================================

TEST_F(PolyphaseDecimatorTest, SNR_1kHz_Sine_HighQuality)
{
    // Test with high-quality filter (longer filter length)
    PolyphaseDecimator<3, 96> decimator(0.45f, 0.05f); // Narrow transition band
    
    const double inputSampleRate = 48000.0;
    const double frequency = 1000.0;
    const double amplitude = 0.7071; // -3dB to avoid clipping
    const int numSamples = 48000; // 1 second of audio
    
    // Generate high-precision 1kHz sine wave
    std::vector<float> inputSignal(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / inputSampleRate;
        inputSignal[i] = amplitude * std::sin(2.0 * M_PI * frequency * t);
    }
    
    // Process through decimator
    std::vector<float> output(numSamples / 3 + 1);
    int outputCount = decimator.processMono(inputSignal.data(), output.data(), numSamples);
    
    // Generate ideal reference signal at 16kHz sample rate
    std::vector<float> reference = generateIdealDecimatedSine(
        frequency, inputSampleRate, amplitude, numSamples, 3
    );
    
    // Skip initial samples for filter settling (group delay compensation)
    int skipSamples = decimator.getGroupDelay() + 10;
    int analysisLength = std::min(outputCount - skipSamples, static_cast<int>(reference.size()) - skipSamples);
    
    ASSERT_GT(analysisLength, 1000) << "Not enough samples for SNR analysis";
    
    // Calculate noise (difference between output and reference)
    std::vector<float> noise(analysisLength);
    std::vector<float> signal(analysisLength);
    
    for (int i = 0; i < analysisLength; ++i)
    {
        signal[i] = reference[i + skipSamples];
        noise[i] = output[i + skipSamples] - reference[i + skipSamples];
    }
    
    // Calculate SNR
    double snr = calculateSNR(signal, noise);
    
    std::cout << "SNR for 1kHz sine wave: " << snr << " dB" << std::endl;
    
    // Expect at least 70 dB SNR
    EXPECT_GE(snr, 70.0) << "SNR requirement not met: " << snr << " dB < 70 dB";
    
    // Additional quality checks
    EXPECT_GT(snr, 60.0) << "SNR is critically low: " << snr << " dB";
    EXPECT_LT(snr, 150.0) << "SNR suspiciously high, possible calculation error: " << snr << " dB";
}

TEST_F(PolyphaseDecimatorTest, SNR_1kHz_Sine_StandardQuality)
{
    // Test with standard filter length (as used in production)
    PolyphaseDecimator<3, 48> decimator;
    
    const double inputSampleRate = 48000.0;
    const double frequency = 1000.0;
    const double amplitude = 0.5;
    const int numSamples = 24000; // 0.5 seconds
    
    // Generate 1kHz sine wave
    std::vector<float> inputSignal(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        double t = static_cast<double>(i) / inputSampleRate;
        inputSignal[i] = amplitude * std::sin(2.0 * M_PI * frequency * t);
    }
    
    std::vector<float> output(numSamples / 3 + 1);
    int outputCount = decimator.processMono(inputSignal.data(), output.data(), numSamples);
    
    std::vector<float> reference = generateIdealDecimatedSine(
        frequency, inputSampleRate, amplitude, numSamples, 3
    );
    
    // Calculate SNR with group delay compensation
    int skipSamples = decimator.getGroupDelay();
    int analysisLength = std::min(outputCount - skipSamples, static_cast<int>(reference.size()) - skipSamples);
    
    std::vector<float> noise(analysisLength);
    std::vector<float> signal(analysisLength);
    
    for (int i = 0; i < analysisLength; ++i)
    {
        signal[i] = reference[i + skipSamples];
        noise[i] = output[i + skipSamples] - reference[i + skipSamples];
    }
    
    double snr = calculateSNR(signal, noise);
    
    std::cout << "SNR for 1kHz sine (standard filter): " << snr << " dB" << std::endl;
    
    // More lenient requirement for standard filter
    EXPECT_GE(snr, 50.0) << "SNR with standard filter too low: " << snr << " dB";
}

TEST_F(PolyphaseDecimatorTest, SNR_Multiple_Frequencies)
{
    PolyphaseDecimator<3, 96> decimator;
    
    // Test frequencies within passband
    std::vector<double> testFrequencies = {440.0, 1000.0, 2000.0, 4000.0, 6000.0};
    
    for (double frequency : testFrequencies)
    {
        const double inputSampleRate = 48000.0;
        const double amplitude = 0.5;
        const int numSamples = 24000;
        
        // Generate sine wave
        std::vector<float> inputSignal(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            double t = static_cast<double>(i) / inputSampleRate;
            inputSignal[i] = amplitude * std::sin(2.0 * M_PI * frequency * t);
        }
        
        std::vector<float> output(numSamples / 3 + 1);
        int outputCount = decimator.processMono(inputSignal.data(), output.data(), numSamples);
        
        std::vector<float> reference = generateIdealDecimatedSine(
            frequency, inputSampleRate, amplitude, numSamples, 3
        );
        
        // Calculate SNR
        int skipSamples = decimator.getGroupDelay();
        int analysisLength = std::min(outputCount - skipSamples, static_cast<int>(reference.size()) - skipSamples);
        
        std::vector<float> noise(analysisLength);
        std::vector<float> signal(analysisLength);
        
        for (int i = 0; i < analysisLength; ++i)
        {
            signal[i] = reference[i + skipSamples];
            noise[i] = output[i + skipSamples] - reference[i + skipSamples];
        }
        
        double snr = calculateSNR(signal, noise);
        
        std::cout << "SNR for " << frequency << " Hz: " << snr << " dB" << std::endl;
        
        // Frequency-dependent SNR requirements
        if (frequency <= 2000.0)
        {
            EXPECT_GE(snr, 60.0) << "SNR too low for " << frequency << " Hz: " << snr << " dB";
        }
        else if (frequency <= 4000.0)
        {
            EXPECT_GE(snr, 50.0) << "SNR too low for " << frequency << " Hz: " << snr << " dB";
        }
        else
        {
            EXPECT_GE(snr, 40.0) << "SNR too low for " << frequency << " Hz: " << snr << " dB";
        }
    }
}

// ============================================================================
// REAL-TIME SAFETY TESTS - No malloc/free in process()
// ============================================================================

TEST_F(PolyphaseDecimatorTest, RealTimeSafety_NoMemoryAllocation)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Prepare test data
    const int numSamples = 1024;
    std::vector<float> input(numSamples, 0.1f);
    std::vector<float> output(numSamples / 3 + 1);
    
    // Reset memory tracking
    MemoryTracker::reset();
    
    // Process audio - this should not allocate any memory
    int outputCount = decimator.processMono(input.data(), output.data(), numSamples);
    
    // Disable tracking
    MemoryTracker::disable();
    
    // Check that no memory was allocated during processing
    size_t allocations = MemoryTracker::getAllocations();
    size_t deallocations = MemoryTracker::getDeallocations();
    
    EXPECT_EQ(allocations, 0) << "Memory allocation detected during processing: " << allocations << " allocations";
    EXPECT_EQ(deallocations, 0) << "Memory deallocation detected during processing: " << deallocations << " deallocations";
    EXPECT_GT(outputCount, 0) << "Processing should produce output";
}

TEST_F(PolyphaseDecimatorTest, RealTimeSafety_StereoToMono_NoMemoryAllocation)
{
    PolyphaseDecimator<3, 48> decimator;
    
    const int numSamples = 1024;
    std::vector<float> leftInput(numSamples, 0.1f);
    std::vector<float> rightInput(numSamples, 0.1f);
    std::vector<float> output(numSamples / 3 + 1);
    
    MemoryTracker::reset();
    
    // Process stereo to mono - should not allocate memory
    int outputCount = decimator.processStereoToMono(
        leftInput.data(), rightInput.data(), output.data(), numSamples
    );
    
    MemoryTracker::disable();
    
    size_t allocations = MemoryTracker::getAllocations();
    size_t deallocations = MemoryTracker::getDeallocations();
    
    EXPECT_EQ(allocations, 0) << "Memory allocation in stereo processing: " << allocations;
    EXPECT_EQ(deallocations, 0) << "Memory deallocation in stereo processing: " << deallocations;
    EXPECT_GT(outputCount, 0) << "Stereo processing should produce output";
}

TEST_F(PolyphaseDecimatorTest, RealTimeSafety_MultipleCallsNoMemoryLeak)
{
    PolyphaseDecimator<3, 48> decimator;
    
    const int numSamples = 512;
    const int numCalls = 100;
    std::vector<float> input(numSamples);
    std::vector<float> output(numSamples / 3 + 1);
    
    // Fill with test signal
    for (int i = 0; i < numSamples; ++i)
    {
        input[i] = 0.1f * std::sin(2.0f * M_PI * i / 48.0f);
    }
    
    MemoryTracker::reset();
    
    // Make multiple processing calls
    for (int call = 0; call < numCalls; ++call)
    {
        int outputCount = decimator.processMono(input.data(), output.data(), numSamples);
        EXPECT_GT(outputCount, 0) << "Call " << call << " should produce output";
    }
    
    MemoryTracker::disable();
    
    size_t allocations = MemoryTracker::getAllocations();
    size_t deallocations = MemoryTracker::getDeallocations();
    
    EXPECT_EQ(allocations, 0) << "Memory allocations in " << numCalls << " calls: " << allocations;
    EXPECT_EQ(deallocations, 0) << "Memory deallocations in " << numCalls << " calls: " << deallocations;
}

TEST_F(PolyphaseDecimatorTest, RealTimeSafety_ResetNoMemoryAllocation)
{
    PolyphaseDecimator<3, 48> decimator;
    
    MemoryTracker::reset();
    
    // Reset should not allocate memory (it should only zero existing buffers)
    decimator.reset();
    
    MemoryTracker::disable();
    
    size_t allocations = MemoryTracker::getAllocations();
    size_t deallocations = MemoryTracker::getDeallocations();
    
    EXPECT_EQ(allocations, 0) << "Memory allocation during reset: " << allocations;
    EXPECT_EQ(deallocations, 0) << "Memory deallocation during reset: " << deallocations;
}

TEST_F(PolyphaseDecimatorTest, RealTimeSafety_ThreadSafety)
{
    // Test that multiple decimators can run concurrently without issues
    const int numThreads = 4;
    const int numSamples = 1024;
    std::atomic<int> successCount{0};
    std::atomic<int> allocationCount{0};
    
    auto processFunction = [&]() {
        PolyphaseDecimator<3, 48> decimator;
        std::vector<float> input(numSamples);
        std::vector<float> output(numSamples / 3 + 1);
        
        // Generate test signal
        for (int i = 0; i < numSamples; ++i)
        {
            input[i] = 0.1f * std::sin(2.0f * M_PI * i / 100.0f);
        }
        
        // Track allocations for this thread
        size_t localAllocsBefore = MemoryTracker::getAllocations();
        
        // Process audio
        int outputCount = decimator.processMono(input.data(), output.data(), numSamples);
        
        size_t localAllocsAfter = MemoryTracker::getAllocations();
        
        if (outputCount > 0 && localAllocsAfter == localAllocsBefore)
        {
            successCount.fetch_add(1);
        }
        
        allocationCount.fetch_add(localAllocsAfter - localAllocsBefore);
    };
    
    MemoryTracker::reset();
    
    // Launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(processFunction);
    }
    
    // Wait for completion
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    MemoryTracker::disable();
    
    EXPECT_EQ(successCount.load(), numThreads) << "Some threads failed processing";
    EXPECT_EQ(allocationCount.load(), 0) << "Memory allocations detected in threaded processing";
}

// ============================================================================
// EXISTING TESTS (keeping all original functionality tests)
// ============================================================================

TEST_F(PolyphaseDecimatorTest, BasicConstruction)
{
    PolyphaseDecimator<3, 48> decimator;
    
    EXPECT_EQ(decimator.kDecimationFactor, 3);
    EXPECT_EQ(decimator.kFilterLength, 48);
    EXPECT_EQ(decimator.kPhaseLength, 16);
    EXPECT_EQ(decimator.getGroupDelay(), 8);
}

TEST_F(PolyphaseDecimatorTest, MonoProcessing)
{
    PolyphaseDecimator<3, 48> decimator;
    
    std::vector<float> output(testSignal.size() / 3 + 1);
    
    int outputCount = decimator.processMono(
        testSignal.data(), output.data(), testSignal.size()
    );
    
    // Should produce approximately 1/3 the number of samples
    EXPECT_GT(outputCount, testSignal.size() / 3 - 10);
    EXPECT_LT(outputCount, testSignal.size() / 3 + 10);
    
    // Output should not be all zeros (signal should pass through filter)
    bool hasNonZero = false;
    for (int i = 0; i < outputCount; ++i)
    {
        if (std::abs(output[i]) > 0.001f)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(PolyphaseDecimatorTest, StereoToMonoProcessing)
{
    PolyphaseDecimator<3, 48> decimator;
    
    std::vector<float> output(testSignal.size() / 3 + 1);
    
    int outputCount = decimator.processStereoToMono(
        stereoLeft.data(), stereoRight.data(), 
        output.data(), stereoLeft.size()
    );
    
    // Should produce approximately 1/3 the number of samples
    EXPECT_GT(outputCount, testSignal.size() / 3 - 10);
    EXPECT_LT(outputCount, testSignal.size() / 3 + 10);
    
    // Output should be average of left and right channels
    // We can't test exact values due to filtering, but should be non-zero
    bool hasNonZero = false;
    for (int i = 0; i < outputCount; ++i)
    {
        if (std::abs(output[i]) > 0.001f)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(PolyphaseDecimatorTest, ConsistentOutputLength)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Test with different input lengths
    std::vector<int> inputLengths = {48, 96, 144, 480, 960, 1440};
    
    for (int inputLength : inputLengths)
    {
        std::vector<float> input(inputLength, 0.5f);
        std::vector<float> output(inputLength / 3 + 1);
        
        int outputCount = decimator.processMono(
            input.data(), output.data(), inputLength
        );
        
        // Output count should be roughly inputLength / 3
        int expectedOutput = inputLength / 3;
        EXPECT_GE(outputCount, expectedOutput - 1);
        EXPECT_LE(outputCount, expectedOutput + 1);
    }
}

TEST_F(PolyphaseDecimatorTest, ResetFunctionality)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Process some data
    std::vector<float> output1(100);
    int count1 = decimator.processMono(testSignal.data(), output1.data(), 300);
    
    // Reset and process again
    decimator.reset();
    std::vector<float> output2(100);
    int count2 = decimator.processMono(testSignal.data(), output2.data(), 300);
    
    // Results should be identical after reset
    EXPECT_EQ(count1, count2);
    
    for (int i = 0; i < std::min(count1, count2); ++i)
    {
        EXPECT_NEAR(output1[i], output2[i], 1e-6f);
    }
}

TEST_F(PolyphaseDecimatorTest, AliasingReduction)
{
    PolyphaseDecimator<3, 48> decimator;
    
    const int sampleRate = 48000;
    const int numSamples = 4800;
    
    // Generate high frequency signal that should be filtered out
    std::vector<float> highFreqSignal(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float t = static_cast<float>(i) / sampleRate;
        // Signal at 20kHz - should be heavily attenuated
        highFreqSignal[i] = 0.5f * std::sin(2.0f * M_PI * 20000.0f * t);
    }
    
    std::vector<float> output(numSamples / 3 + 1);
    int outputCount = decimator.processMono(
        highFreqSignal.data(), output.data(), numSamples
    );
    
    // Calculate RMS of output - should be much smaller than input due to filtering
    float outputRMS = 0.0f;
    for (int i = 0; i < outputCount; ++i)
    {
        outputRMS += output[i] * output[i];
    }
    outputRMS = std::sqrt(outputRMS / outputCount);
    
    // High frequency signal should be significantly attenuated
    EXPECT_LT(outputRMS, 0.1f); // Less than 20% of original amplitude
}

TEST_F(PolyphaseDecimatorTest, DCResponse)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // DC signal should pass through with minimal attenuation
    std::vector<float> dcSignal(1440, 1.0f); // 30ms of DC at 48kHz
    std::vector<float> output(480);
    
    int outputCount = decimator.processMono(
        dcSignal.data(), output.data(), dcSignal.size()
    );
    
    // Allow some samples for filter settling
    float avgOutput = 0.0f;
    int startSample = 20; // Skip initial samples for filter settling
    for (int i = startSample; i < outputCount; ++i)
    {
        avgOutput += output[i];
    }
    avgOutput /= (outputCount - startSample);
    
    // DC response should be close to 1.0 (minimal attenuation)
    EXPECT_GT(avgOutput, 0.8f);
    EXPECT_LT(avgOutput, 1.2f);
}

TEST_F(PolyphaseDecimatorTest, DifferentDecimationFactors)
{
    // Test with different decimation factors
    {
        PolyphaseDecimator<2, 32> decimator2;
        std::vector<float> output(testSignal.size() / 2 + 1);
        int count = decimator2.processMono(
            testSignal.data(), output.data(), testSignal.size()
        );
        EXPECT_GT(count, testSignal.size() / 2 - 10);
        EXPECT_LT(count, testSignal.size() / 2 + 10);
    }
    
    {
        PolyphaseDecimator<4, 64> decimator4;
        std::vector<float> output(testSignal.size() / 4 + 1);
        int count = decimator4.processMono(
            testSignal.data(), output.data(), testSignal.size()
        );
        EXPECT_GT(count, testSignal.size() / 4 - 10);
        EXPECT_LT(count, testSignal.size() / 4 + 10);
    }
}

TEST_F(PolyphaseDecimatorTest, NoiseTest)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Generate white noise
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    std::vector<float> noise(4800);
    for (size_t i = 0; i < noise.size(); ++i)
    {
        noise[i] = dis(gen);
    }
    
    std::vector<float> output(noise.size() / 3 + 1);
    int outputCount = decimator.processMono(
        noise.data(), output.data(), noise.size()
    );
    
    // Output should be stable (no NaN or infinite values)
    for (int i = 0; i < outputCount; ++i)
    {
        EXPECT_TRUE(std::isfinite(output[i]));
        EXPECT_FALSE(std::isnan(output[i]));
    }
    
    // Output amplitude should be reasonable
    float maxAbs = 0.0f;
    for (int i = 0; i < outputCount; ++i)
    {
        maxAbs = std::max(maxAbs, std::abs(output[i]));
    }
    
    // Should not amplify excessively
    EXPECT_LT(maxAbs, 2.0f);
}

TEST_F(PolyphaseDecimatorTest, StreamingConsistency)
{
    // Test that processing in chunks gives same result as processing all at once
    PolyphaseDecimator<3, 48> decimator1;
    PolyphaseDecimator<3, 48> decimator2;
    
    // Process all at once
    std::vector<float> outputAll(testSignal.size() / 3 + 1);
    int countAll = decimator1.processMono(
        testSignal.data(), outputAll.data(), testSignal.size()
    );
    
    // Process in chunks
    std::vector<float> outputChunked(testSignal.size() / 3 + 1);
    int totalChunked = 0;
    const int chunkSize = 160; // Process in 160-sample chunks
    
    for (size_t offset = 0; offset < testSignal.size(); offset += chunkSize)
    {
        int samplesThisChunk = std::min(chunkSize, 
                                       static_cast<int>(testSignal.size() - offset));
        
        int chunkOutput = decimator2.processMono(
            testSignal.data() + offset,
            outputChunked.data() + totalChunked,
            samplesThisChunk
        );
        
        totalChunked += chunkOutput;
    }
    
    // Results should be very similar
    EXPECT_EQ(countAll, totalChunked);
    
    for (int i = 0; i < std::min(countAll, totalChunked); ++i)
    {
        EXPECT_NEAR(outputAll[i], outputChunked[i], 1e-5f);
    }
}

TEST_F(PolyphaseDecimatorTest, MonoProcessing)
{
    PolyphaseDecimator<3, 48> decimator;
    
    std::vector<float> output(testSignal.size() / 3 + 1);
    
    int outputCount = decimator.processMono(
        testSignal.data(), output.data(), testSignal.size()
    );
    
    // Should produce approximately 1/3 the number of samples
    EXPECT_GT(outputCount, testSignal.size() / 3 - 10);
    EXPECT_LT(outputCount, testSignal.size() / 3 + 10);
    
    // Output should not be all zeros (signal should pass through filter)
    bool hasNonZero = false;
    for (int i = 0; i < outputCount; ++i)
    {
        if (std::abs(output[i]) > 0.001f)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(PolyphaseDecimatorTest, StereoToMonoProcessing)
{
    PolyphaseDecimator<3, 48> decimator;
    
    std::vector<float> output(testSignal.size() / 3 + 1);
    
    int outputCount = decimator.processStereoToMono(
        stereoLeft.data(), stereoRight.data(), 
        output.data(), stereoLeft.size()
    );
    
    // Should produce approximately 1/3 the number of samples
    EXPECT_GT(outputCount, testSignal.size() / 3 - 10);
    EXPECT_LT(outputCount, testSignal.size() / 3 + 10);
    
    // Output should be average of left and right channels
    // We can't test exact values due to filtering, but should be non-zero
    bool hasNonZero = false;
    for (int i = 0; i < outputCount; ++i)
    {
        if (std::abs(output[i]) > 0.001f)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(PolyphaseDecimatorTest, ConsistentOutputLength)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Test with different input lengths
    std::vector<int> inputLengths = {48, 96, 144, 480, 960, 1440};
    
    for (int inputLength : inputLengths)
    {
        std::vector<float> input(inputLength, 0.5f);
        std::vector<float> output(inputLength / 3 + 1);
        
        int outputCount = decimator.processMono(
            input.data(), output.data(), inputLength
        );
        
        // Output count should be roughly inputLength / 3
        int expectedOutput = inputLength / 3;
        EXPECT_GE(outputCount, expectedOutput - 1);
        EXPECT_LE(outputCount, expectedOutput + 1);
    }
}

TEST_F(PolyphaseDecimatorTest, ResetFunctionality)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Process some data
    std::vector<float> output1(100);
    int count1 = decimator.processMono(testSignal.data(), output1.data(), 300);
    
    // Reset and process again
    decimator.reset();
    std::vector<float> output2(100);
    int count2 = decimator.processMono(testSignal.data(), output2.data(), 300);
    
    // Results should be identical after reset
    EXPECT_EQ(count1, count2);
    
    for (int i = 0; i < std::min(count1, count2); ++i)
    {
        EXPECT_NEAR(output1[i], output2[i], 1e-6f);
    }
}

TEST_F(PolyphaseDecimatorTest, AliasingReduction)
{
    PolyphaseDecimator<3, 48> decimator;
    
    const int sampleRate = 48000;
    const int numSamples = 4800;
    
    // Generate high frequency signal that should be filtered out
    std::vector<float> highFreqSignal(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float t = static_cast<float>(i) / sampleRate;
        // Signal at 20kHz - should be heavily attenuated
        highFreqSignal[i] = 0.5f * std::sin(2.0f * M_PI * 20000.0f * t);
    }
    
    std::vector<float> output(numSamples / 3 + 1);
    int outputCount = decimator.processMono(
        highFreqSignal.data(), output.data(), numSamples
    );
    
    // Calculate RMS of output - should be much smaller than input due to filtering
    float outputRMS = 0.0f;
    for (int i = 0; i < outputCount; ++i)
    {
        outputRMS += output[i] * output[i];
    }
    outputRMS = std::sqrt(outputRMS / outputCount);
    
    // High frequency signal should be significantly attenuated
    EXPECT_LT(outputRMS, 0.1f); // Less than 20% of original amplitude
}

TEST_F(PolyphaseDecimatorTest, DCResponse)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // DC signal should pass through with minimal attenuation
    std::vector<float> dcSignal(1440, 1.0f); // 30ms of DC at 48kHz
    std::vector<float> output(480);
    
    int outputCount = decimator.processMono(
        dcSignal.data(), output.data(), dcSignal.size()
    );
    
    // Allow some samples for filter settling
    float avgOutput = 0.0f;
    int startSample = 20; // Skip initial samples for filter settling
    for (int i = startSample; i < outputCount; ++i)
    {
        avgOutput += output[i];
    }
    avgOutput /= (outputCount - startSample);
    
    // DC response should be close to 1.0 (minimal attenuation)
    EXPECT_GT(avgOutput, 0.8f);
    EXPECT_LT(avgOutput, 1.2f);
}

TEST_F(PolyphaseDecimatorTest, DifferentDecimationFactors)
{
    // Test with different decimation factors
    {
        PolyphaseDecimator<2, 32> decimator2;
        std::vector<float> output(testSignal.size() / 2 + 1);
        int count = decimator2.processMono(
            testSignal.data(), output.data(), testSignal.size()
        );
        EXPECT_GT(count, testSignal.size() / 2 - 10);
        EXPECT_LT(count, testSignal.size() / 2 + 10);
    }
    
    {
        PolyphaseDecimator<4, 64> decimator4;
        std::vector<float> output(testSignal.size() / 4 + 1);
        int count = decimator4.processMono(
            testSignal.data(), output.data(), testSignal.size()
        );
        EXPECT_GT(count, testSignal.size() / 4 - 10);
        EXPECT_LT(count, testSignal.size() / 4 + 10);
    }
}

TEST_F(PolyphaseDecimatorTest, NoiseTest)
{
    PolyphaseDecimator<3, 48> decimator;
    
    // Generate white noise
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    std::vector<float> noise(4800);
    for (size_t i = 0; i < noise.size(); ++i)
    {
        noise[i] = dis(gen);
    }
    
    std::vector<float> output(noise.size() / 3 + 1);
    int outputCount = decimator.processMono(
        noise.data(), output.data(), noise.size()
    );
    
    // Output should be stable (no NaN or infinite values)
    for (int i = 0; i < outputCount; ++i)
    {
        EXPECT_TRUE(std::isfinite(output[i]));
        EXPECT_FALSE(std::isnan(output[i]));
    }
    
    // Output amplitude should be reasonable
    float maxAbs = 0.0f;
    for (int i = 0; i < outputCount; ++i)
    {
        maxAbs = std::max(maxAbs, std::abs(output[i]));
    }
    
    // Should not amplify excessively
    EXPECT_LT(maxAbs, 2.0f);
}

TEST_F(PolyphaseDecimatorTest, StreamingConsistency)
{
    // Test that processing in chunks gives same result as processing all at once
    PolyphaseDecimator<3, 48> decimator1;
    PolyphaseDecimator<3, 48> decimator2;
    
    // Process all at once
    std::vector<float> outputAll(testSignal.size() / 3 + 1);
    int countAll = decimator1.processMono(
        testSignal.data(), outputAll.data(), testSignal.size()
    );
    
    // Process in chunks
    std::vector<float> outputChunked(testSignal.size() / 3 + 1);
    int totalChunked = 0;
    const int chunkSize = 160; // Process in 160-sample chunks
    
    for (size_t offset = 0; offset < testSignal.size(); offset += chunkSize)
    {
        int samplesThisChunk = std::min(chunkSize, 
                                       static_cast<int>(testSignal.size() - offset));
        
        int chunkOutput = decimator2.processMono(
            testSignal.data() + offset,
            outputChunked.data() + totalChunked,
            samplesThisChunk
        );
        
        totalChunked += chunkOutput;
    }
    
    // Results should be very similar
    EXPECT_EQ(countAll, totalChunked);
    
    for (int i = 0; i < std::min(countAll, totalChunked); ++i)
    {
        EXPECT_NEAR(outputAll[i], outputChunked[i], 1e-5f);
    }
}
 