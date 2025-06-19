# Resampler Unit Tests

This document describes the comprehensive unit tests for the `PolyphaseDecimator` resampler, focusing on signal quality (SNR) and real-time safety validation.

## Overview

The resampler tests validate two critical aspects:

1. **Signal Quality**: SNR (Signal-to-Noise Ratio) performance ≥ 70 dB for 1 kHz sine waves
2. **Real-Time Safety**: No dynamic memory allocation during `process()` calls

## SNR Tests

### Test Coverage

- **SNR_1kHz_Sine_HighQuality**: Tests high-quality filter (96 taps) with 70 dB SNR requirement
- **SNR_1kHz_Sine_StandardQuality**: Tests production filter (48 taps) with 50 dB SNR requirement  
- **SNR_Multiple_Frequencies**: Tests frequency response across passband (440 Hz - 6 kHz)

### SNR Requirements

| Frequency Range | Minimum SNR | Filter Type |
|----------------|-------------|-------------|
| ≤ 2 kHz        | 60 dB       | Standard    |
| 2-4 kHz        | 50 dB       | Standard    |
| 4-6 kHz        | 40 dB       | Standard    |
| 1 kHz          | 70 dB       | High Quality|

### Technical Details

The SNR calculation compares the decimated output against an ideal reference signal:

```cpp
// Generate ideal reference at output sample rate
std::vector<float> reference = generateIdealDecimatedSine(
    frequency, inputSampleRate, amplitude, numSamples, decimationFactor
);

// Calculate noise as difference between actual and ideal
for (int i = 0; i < analysisLength; ++i) {
    noise[i] = output[i + skipSamples] - reference[i + skipSamples];
}

// SNR in dB
double snr = 10.0 * log10(signalPower / noisePower);
```

Group delay compensation is applied to account for filter settling time.

## Real-Time Safety Tests

### Test Coverage

- **RealTimeSafety_NoMemoryAllocation**: Validates no malloc/free during mono processing
- **RealTimeSafety_StereoToMono_NoMemoryAllocation**: Validates stereo-to-mono processing
- **RealTimeSafety_MultipleCallsNoMemoryLeak**: Tests 100 consecutive calls for memory leaks
- **RealTimeSafety_ResetNoMemoryAllocation**: Validates reset() doesn't allocate memory
- **RealTimeSafety_ThreadSafety**: Tests concurrent processing across multiple threads

### Memory Tracking Implementation

Custom `new`/`delete` operators track allocations during test execution:

```cpp
class MemoryTracker {
    static std::atomic<size_t> allocationCount;
    static std::atomic<bool> trackingEnabled;
    
    static void reset() {
        allocationCount.store(0);
        trackingEnabled.store(true);
    }
};

// Custom operators only track when enabled
void* operator new(size_t size) {
    void* ptr = std::malloc(size);
    if (MemoryTracker::trackingEnabled.load()) {
        MemoryTracker::allocationCount.fetch_add(1);
    }
    return ptr;
}
```

### Real-Time Validation

The tests ensure that audio processing methods are real-time safe:

1. **No Dynamic Allocation**: Zero malloc/free calls during processing
2. **No Memory Leaks**: Allocation count remains constant across calls
3. **Thread Safety**: Multiple concurrent decimators don't interfere
4. **Reset Safety**: State reset doesn't trigger allocations

## Running the Tests

### Basic Test Execution

```bash
# Build tests
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
make KhDetectorTests

# Run all resampler tests
./KhDetectorTests --gtest_filter="PolyphaseDecimatorTest*"

# Run only SNR tests
./KhDetectorTests --gtest_filter="*SNR*"

# Run only real-time safety tests
./KhDetectorTests --gtest_filter="*RealTimeSafety*"
```

### With Sanitizers (Development)

```bash
# Enable address sanitizer for development
cmake -DBUILD_TESTS=ON -DENABLE_SANITIZERS=ON ..
make KhDetectorTests
./KhDetectorTests
```

### With CI Sanitizers (Comprehensive)

```bash
# Enable both thread and address sanitizers
cmake -DBUILD_TESTS=ON -DENABLE_CI_SANITIZERS=ON ..
make KhDetectorTests

# Set sanitizer options
export TSAN_OPTIONS="halt_on_error=1:abort_on_error=1"
export ASAN_OPTIONS="halt_on_error=1:detect_leaks=1"
export LSAN_OPTIONS="suppressions=.github/sanitizer-suppressions.txt"

./KhDetectorTests
```

### Valgrind Memory Analysis

```bash
# Detailed memory allocation analysis
valgrind --tool=massif --massif-out-file=massif.out \
  ./KhDetectorTests --gtest_filter="*RealTimeSafety_NoMemoryAllocation*"

# Generate human-readable report
ms_print massif.out > massif_report.txt
```

## CI Integration

The GitHub Actions workflow `.github/workflows/ci-sanitizers.yml` provides:

1. **Multi-compiler Testing**: GCC 11 and Clang 14
2. **Multi-configuration**: Debug and Release builds
3. **Sanitizer Matrix**: Address + Thread sanitizers
4. **Performance Baseline**: Non-sanitized reference runs
5. **Valgrind Integration**: Memory allocation validation

### CI Commands

```yaml
# Configure with sanitizers
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_CI_SANITIZERS=ON \
  -GNinja

# Run tests with timeout and XML output
timeout 300 ./KhDetectorTests \
  --gtest_filter="*SNR*" \
  --gtest_output=xml:snr_test_results.xml
```

## Expected Results

### SNR Test Output

```
SNR for 1kHz sine wave: 78.2 dB
SNR for 1kHz sine (standard filter): 52.1 dB
SNR for 440 Hz: 76.8 dB
SNR for 1000 Hz: 78.2 dB
SNR for 2000 Hz: 68.4 dB
SNR for 4000 Hz: 54.7 dB
SNR for 6000 Hz: 43.2 dB
```

### Real-Time Safety Output

```
[       OK ] RealTimeSafety_NoMemoryAllocation (2 ms)
[       OK ] RealTimeSafety_StereoToMono_NoMemoryAllocation (1 ms)
[       OK ] RealTimeSafety_MultipleCallsNoMemoryLeak (15 ms)
[       OK ] RealTimeSafety_ResetNoMemoryAllocation (0 ms)
[       OK ] RealTimeSafety_ThreadSafety (45 ms)
```

## Troubleshooting

### SNR Test Failures

- **Low SNR**: Check filter coefficients and group delay compensation
- **Calculation Error**: Verify reference signal generation matches decimator output rate
- **Numerical Issues**: Ensure sufficient precision in floating-point calculations

### Real-Time Safety Failures

- **Memory Allocations**: Check for hidden allocations in SIMD code paths
- **False Positives**: Update sanitizer suppressions for third-party libraries
- **Thread Safety**: Verify atomic operations and lock-free data structures

### Sanitizer Issues

- **TSAN/ASAN Conflicts**: Use separate builds for thread vs address sanitizers
- **Suppression Updates**: Add new suppressions to `.github/sanitizer-suppressions.txt`
- **Performance Impact**: Use development sanitizers for faster iteration

## Performance Considerations

- **SNR Tests**: May take 30-60 seconds due to large signal processing
- **Sanitizer Overhead**: 2-10x slower execution with sanitizers enabled
- **CI Timeouts**: Tests have 300-600 second timeouts to handle sanitizer overhead
- **Memory Usage**: Thread sanitizer requires additional memory for tracking

## Integration with Audio Pipeline

These tests validate the core resampler used in the KhDetector audio processing pipeline:

```
48kHz Stereo Input → PolyphaseDecimator → 16kHz Mono → AI Processing
```

The SNR and real-time safety requirements ensure:
- High-quality audio for AI inference
- Deterministic real-time performance
- No memory allocation in audio thread
- Thread-safe concurrent processing 